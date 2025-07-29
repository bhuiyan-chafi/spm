#include <iostream>
#include <random>
#include <cstdint>
#include <vector>
#include "hpc_helpers.hpp"


void checkResult(std::vector<float> &C, std::vector<float> &Cc,
                 uint64_t m, uint64_t n) {
    for (uint64_t i = 0; i < m; i++)
        for (uint64_t j = 0; j < n; j++) {
            if (std::abs(C[i*n+j] - Cc[i*n+j]) > 1e-4f) {
                std::printf("Error %f, expected %f [%ld,%ld]\n", C[i*n+j], Cc[i*n+j], i, j);
                return;	
            }
        }
}


void init(float * data, uint64_t length) {

    std::mt19937 engine(42);
    std::uniform_real_distribution<float> density(-1, 1);

    for (uint64_t i = 0; i < length; i++)
        data[i] = density(engine);
}

int main (int argc, char *argv[]) {
	uint64_t def_m=15, def_n=15, def_l=5;
	uint64_t T=16;
	if (argc != 1 && argc != 5) {
		std::printf("use: %s m n l t\n", argv[0]);
		return -1;
	}
	if (argc > 1) {
		def_m = std::stol(argv[1]);
		def_n = std::stol(argv[2]);
		def_l = std::stol(argv[3]);
		T     = std::stol(argv[4]);
	}

    // matrix shapes
    const uint64_t m = 1 << def_m;
    const uint64_t n = 1 << def_n;
    const uint64_t l = 1 << def_l;
	

    // sum_k A_ik * B_kj = sum_k A_ik * B^t_jk = C_ij
    std::vector<float> A (m*l, 0); // m x l
    std::vector<float> B (l*n, 0); // l x n
    std::vector<float> Bt(n*l, 0); // n x l
    std::vector<float> C (m*n, 0); // m x n
    std::vector<float> Ccheck (m*n, 0); // m x n

    init(A.data(), m*l);
    init(B.data(), l*n);

	
    TIMERSTART(transpose_and_mult);
    for (uint64_t k = 0; k < l; k++)
        for (uint64_t j = 0; j < n; j++)
            Bt[j*l+k] = B[k*n+j];
    for (uint64_t i = 0; i < m; i++)
        for (uint64_t j = 0; j < n; j++) {
            float accum = 0;
            for (uint64_t k = 0; k < l; k++)
                accum += A[i*l+k]*Bt[j*l+k];
            Ccheck[i*n+j] = accum;
	    }

    TIMERSTOP(transpose_and_mult);

	TIMERSTART(tiled_mult);
    // Loop over tiles
    for (uint64_t i = 0; i < m; i += T) {
        for (uint64_t j = 0; j < n; j += T) {
            for (uint64_t k = 0; k < l; k += T) {

                // Compute the multiplication for the current tile
                for (uint64_t ii = i; ii < std::min(i + T,m); ++ii) {
                    for (uint64_t jj = j; jj < std::min(j + T,n); ++jj) {
                        float sum = 0.0;
                        for (uint64_t kk = k; kk < std::min(k + T,l); ++kk) {
                            sum += A[ii * l + kk] * B[kk * n + jj];
                        }
                        C[ii * n + jj] += sum;
                    }
                }
            }
        }
    }
    TIMERSTOP(tiled_mult);
    checkResult(C, Ccheck, m,n);
	
}
