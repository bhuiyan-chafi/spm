/**
 * Root scatters the data and gathers the results.
 * Each process computes the sum of its part, locally.
 * std::iota() is used to fill a vector with sequentially increasing values. It is defined in the <numeric> header.
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
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const int N = 8; // must be divisible by size for this tiny demo
    std::vector<int> full;
    if (rank == 0)
    {
        spdlog::info("Scattering from root process {}", rank);
        full.resize(N);
        std::iota(full.begin(), full.end(), 1); // [1..N]
        spdlog::info("Full data: {}", fmt::join(full, ", "));
    }

    int chunk = N / size;
    std::vector<int> part(chunk);
    MPI_Scatter(full.data(), chunk, MPI_INT, part.data(), chunk, MPI_INT, 0, MPI_COMM_WORLD);

    // local compute: sum my part
    int local_sum = 0;
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