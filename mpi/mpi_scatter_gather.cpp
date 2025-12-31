/**
 * Root scatters the data and gathers the results.
 * Each process computes the sum of its part, locally.
 * std::iota() is used to fill a vector with sequentially increasing values. It is defined in the <numeric> header.
 * Debug example for: mpirun -np 2 ./mpi_scatter_gather
 */
#include <mpi.h>
#include <vector>
#include <numeric>
#include <iostream>
#include <spdlog/spdlog.h>

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // 0,1
    MPI_Comm_size(MPI_COMM_WORLD, &size); // 2

    const int N = 8;       // must be divisible by size=2 for this tiny demo
    std::vector<int> full; // now there is no default size at the beginning
    if (rank == 0)
    {
        spdlog::info("Scattering from root process {}", rank);
        full.resize(N);                         // full size is 8 now
        std::iota(full.begin(), full.end(), 1); // [1..N(8)], start from 1 and increase by 1
        spdlog::info("Full data: {}", fmt::join(full, ", "));
    }

    int chunk = N / size;         // 4
    std::vector<int> part(chunk); // each part size is 4
    // scatter full to parts, we have 2 parts, each of size 4
    MPI_Scatter(full.data(), chunk, MPI_INT, part.data(), chunk, MPI_INT, 0, MPI_COMM_WORLD);

    // local compute: sum my part
    int local_sum = 0;
    /**
     * each process has its own part and computes its own local_sum
     * process 0 has part [1,2,3,4], local_sum = 10
     * process 1 has part [5,6,7,8], local_sum = 26
     */
    for (int v : part)
        local_sum += v;
    spdlog::info("Process {} has local sum = {}", rank, local_sum);
    std::vector<int> gathered(size);
    MPI_Gather(&local_sum, 1, MPI_INT, gathered.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0)
    {
        spdlog::info("Gather started at root process {}", rank);
        int total = 0;
        for (int s : gathered)
            total += s;
        std::cout << "Total sum = " << total << "\n"; // should be N*(N+1)/2
    }
    MPI_Finalize();
    return 0;
}