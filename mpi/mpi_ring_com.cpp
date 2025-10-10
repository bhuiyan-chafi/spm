/**
 * Also Point-to-point communication.
 * The ring send is also a blocking operation.
 * Debugged in the image, see README.md for details.
 */
#include <mpi.h>
#include <iostream>
#include <spdlog/spdlog.h>

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int send_to = (rank + 1) % size, recv_from = (rank - 1 + size) % size;
    int val = rank; // each rank starts with its id
    MPI_Send(&val, 1, MPI_INT, send_to, 0, MPI_COMM_WORLD);
    spdlog::info("Rank {} sent {} to {}", rank, val, send_to);
    int got;
    MPI_Recv(&got, 1, MPI_INT, recv_from, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    spdlog::info("Rank {} received {} from {}", rank, got, recv_from);

    MPI_Finalize();
    return 0;
}