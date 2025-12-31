/**
 * A Point to Point communication example.
 * Called "ping-pong".
 * Debugged in the image, see README.md for details.
 */
#include <mpi.h>
#include <iostream>
int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int x;
    if (rank == 0)
    {
        x = 7;
        MPI_Send(&x, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        MPI_Recv(&x, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        std::cout << "Rank 0 got back " << x << "\n";
    }
    else if (rank == 1)
    {
        MPI_Recv(&x, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        x *= 10;
        MPI_Send(&x, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        std::cout << "Rank 1 processed " << x << "\n";
    }
    MPI_Finalize();
    return 0;
}