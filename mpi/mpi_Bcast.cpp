/**
 * The collective approach of communication.
 * Broadcasts a message from the process with rank "root" to all other processes of the communicator.
 * The root also participates in the communication, but it's buffer is already loaded with the data; so, it doesn't load it again.
 */

#include <mpi.h>
#include <iostream>
#include <spdlog/spdlog.h>

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int root = 0, x = (rank == root ? 1234 : -1);
    MPI_Bcast(&x, 1, MPI_INT, root, MPI_COMM_WORLD);
    spdlog::info("Process {} has x = {}", rank, x);
    MPI_Finalize();
    return 0;
}