// Mandelbrot set.
// Static scheduling using block (b) and block-cyclic with chunksize distributions.
//
#include <iostream>
#include <complex>
#include <mutex>
#include <thread>
#include <vector>
#include <cassert>
#include <atomic>
#include <functional>
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

/* Coordinates of the bounding box of the Mandelbrot set */
const double XMIN = -2.3, XMAX = 1.0;
const double SCALE = (XMAX - XMIN)*YSIZE / XSIZE;
const double YMIN = -SCALE/2, YMAX = SCALE/2;

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



int main(int argc, char *argv[]) {
	if (argc<2 || argc>4) {
		std::printf("usage: %s b|bc [nthreads] [chunksize]\n", argv[0]);
		return -1;
	}
	std::string policy{argv[1]};
	if (policy != "b" && policy != "bc") {
		std::printf("invalid policy. Use 'b' or 'bc'\n");
		return -1;
	}

	int nthreads=std::thread::hardware_concurrency();
	int chunksize=8;

	if (argc >= 3) {
		nthreads=std::stol(argv[2]);
		if (nthreads < 0) {
			std::printf("usage: %s b|bc [nthreads>0] [chunksize]\n", argv[0]);
			return -1;
		}
	}
	if (argc == 4) {
		chunksize=std::stol(argv[3]);
		if (chunksize <= 0) {
			std::printf("usage: %s b|bc [nthreads] [chunksize>0]\n", argv[0]);
			return -1;
		}
	}

	DISPLAY(gfx_open(XSIZE, YSIZE, "Mandelbrot Set") );

	auto block_cyclic = [&](int threadid) {
		const int offset = threadid * chunksize;
		const int stride = nthreads * chunksize;
		
		std::vector<int> results(XSIZE);
		for(int lower=offset; lower<YSIZE; lower += stride) {
			const int upper = std::min(lower+chunksize, YSIZE);

			for(int y=lower; y<upper;++y) {				
				const double im = YMAX - (YMAX - YMIN) * y / (YSIZE - 1);
				for (int x = 0; x < XSIZE; ++x) {
					const double re = XMIN + (XMAX - XMIN) * x / (XSIZE - 1);
					results[x] = iterate(std::complex<double>(re, im));
				}
			
				// now display all pixels of a row 
				DISPLAY(drawrowpixels(y,results));
			}
		}
	};
	auto block_based = [&](int threadid) {
		int rows_per_thread = YSIZE / nthreads;
		int rem             = YSIZE % nthreads;		
		const int lower = threadid * rows_per_thread + std::min(threadid, rem);
		const int upper = lower + rows_per_thread + (threadid<rem?1:0);
		
		std::vector<int> results(XSIZE);
		for (int y = lower; y < upper; ++y) {
			const double im = YMAX - (YMAX - YMIN) * y / (YSIZE - 1);
			for (int x = 0; x < XSIZE; ++x) {
				const double re = XMIN + (XMAX - XMIN) * x / (XSIZE - 1);
				results[x] = iterate(std::complex<double>(re, im));
			}
			DISPLAY(drawrowpixels(y, results));
		}		
	};
	std::function<void(int)> distribution;
	if (policy=="b") {
		distribution= block_based;
	} else {
		distribution= block_cyclic;
	}
	
	std::vector<std::thread> threads;

	TIMERSTART(mandel_par);
	for(int id=0;id<nthreads;++id)  {
		threads.emplace_back([&, threadid=id] {
			distribution(threadid);
		});
	}
	for(int i=0;i<nthreads;++i)
		threads[i].join();
	TIMERSTOP(mandel_par);

	DISPLAY(std::cout << "Click to finish\n"; gfx_wait());
}
