// Mandelbrot set.
// Dynamic scheduling using the mutex-baesd (atomic-based) iteration space.
//
#include <iostream>
#include <complex>
#include <mutex>
#include <thread>
#include <vector>
#include <cassert>
#include <atomic>
#include <hpc_helpers.hpp>
#include "gfx.h"

#if defined(NO_DISPLAY)
#define DISPLAY(X)
#else
#define DISPLAY(X) X
#endif


// a very large value
const int max_iter = 50000;  

// size in pixels of the picture window
const int XSIZE = 1280;
const int YSIZE = 800;

// Coordinates of the bounding box of the Mandelbrot set
const double XMIN = -2.3, XMAX = 1.0;
const double SCALE = (XMAX - XMIN)*YSIZE / XSIZE;
const double YMIN = -SCALE/2, YMAX = SCALE/2;

// Mutex used to serialize the display of row pixels
std::mutex display_mutex;

struct pixel {
    int r, g, b;
};

const pixel colors[] = {
    { 66,  30,  15}, 
    { 25,   7,  26},
    {  9,   1,  47},
    {  4,   4,  73},
    {  0,   7, 100},
    { 12,  44, 138},
    { 24,  82, 177},
    { 57, 125, 209},
    {134, 181, 229},
    {211, 236, 248},
    {241, 233, 191},
    {248, 201,  95},
    {255, 170,   0},
    {204, 128,   0},
    {153,  87,   0},
    {106,  52,   3} };
const int NCOLORS = sizeof(colors)/sizeof(pixel);


//  z_(0)   = 0;
//  z_(n+1) = z_n * z_n + (cx + i*cy);
//
// iterates until ||z_n|| > 2, or max_iter
//
int iterate(const std::complex<double>& c) {
	std::complex<double> z=0;
	int iter = 0;
	while(iter < max_iter && std::abs(z) <= 2.0) {
		z = z*z + c;
		++iter;
	}
	return iter;
}

void drawrowpixels(int y, const std::vector<int>& row) {
	std::lock_guard<std::mutex> lock(display_mutex);
	int row_pixels[XSIZE];
	for (int x = 0; x < XSIZE; ++x) {
		int r = 0, g = 0, b = 0;
		if (row[x] < max_iter) {
			int m = row[x] % NCOLORS;
			r = colors[m].r;
			g = colors[m].g;
			b = colors[m].b;
		}
		// Construct a 24-bit pixel value (BGR ordering as used in gfx_color)
		row_pixels[x] = ((b & 0xff) | ((g & 0xff) << 8) | ((r & 0xff) << 16));
	}
	// Draw the entire row at once.
	gfx_draw_row(y, row_pixels, XSIZE);
}


#if 1
struct IterationSpace {
	IterationSpace(int chunksize):chunksize(chunksize) {}
	int next_row() {
		std::lock_guard<std::mutex> lock(mtx);
		y+=chunksize;
		if (y >=YSIZE) return -1;
		return y;
	}
	int y=0;
	int chunksize;
	std::mutex mtx;
};
#else
// lock-free version using atomic operations
struct IterationSpace {
	IterationSpace(int chunksize):chunksize(chunksize) {}
	int next_row() {
		int _y = y.fetch_add(chunksize,std::memory_order_relaxed);
		if (_y >=YSIZE) return -1;
		return _y+chunksize;
	}
	int chunksize;
	std::atomic<int> y{0};
};
#endif	


int main(int argc, char *argv[]) {
	int nthreads=std::thread::hardware_concurrency();
	int chunksize=1;
	if (argc>3) {
		std::printf("usage: %s [nthreads=%d] [chunksize=%d]\n", argv[0], nthreads, chunksize);
		return -1;
	}
	if (argc >= 2) {
		nthreads=std::stol(argv[1]);
		if (nthreads < 0) {
			std::printf("invalid nthreads.\n");
			std::printf("usage: %s nthreads=%d [chunksize=%d]\n", argv[0], nthreads, chunksize);
			return -1;
		}
	}
	if (argc == 3) {
		chunksize=std::stol(argv[2]);
		if (chunksize < 0) {
			std::printf("invalid chunksize.\n");
			std::printf("usage: %s [nthreads=%d] chunksize=%d\n", argv[0], nthreads, chunksize);
			return -1;
		}
	}

	DISPLAY(gfx_open(XSIZE, YSIZE, "Mandelbrot Set") );

	IterationSpace iterations(chunksize);

	std::vector<std::thread> threads;

	TIMERSTART(mandel_par);
	for(int id=0;id<nthreads;++id)
		threads.emplace_back([&]() {

			int lower;
			std::vector<int> results(XSIZE);
			while((lower=iterations.next_row())>=0) {
				const int upper = std::min(lower+chunksize, YSIZE);
				for(int y=lower; y<upper; ++y) {
					const double im = YMAX - (YMAX - YMIN) * y / (YSIZE - 1);
					for (int x = 0; x < XSIZE; ++x) {
						const double re = XMIN + (XMAX - XMIN) * x / (XSIZE - 1);
						results[x] = iterate(std::complex<double>(re, im));
					}
					
					// now display all pixels of a row 
					DISPLAY(drawrowpixels(y,results));				
				}
			}
		});
	
	for(int i=0;i<nthreads;++i)
		threads[i].join();
	TIMERSTOP(mandel_par);

	DISPLAY(std::cout << "Click to finish\n"; gfx_wait());
}
