#include <mpi.h>
#include <iostream>
int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int msg = 0;
    MPI_Request reqs[2];

    if (rank == 0)
    {
        msg = 42;
        MPI_Isend(&msg, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &reqs[0]);
        MPI_Irecv(&msg, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &reqs[1]);
        MPI_Waitall(2, reqs, MPI_STATUSES_IGNORE);
        std::cout << "Rank 0 got back " << msg << "\n";
    }
    else if (rank == 1)
    {
        MPI_Irecv(&msg, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &reqs[0]);
        MPI_Isend(&msg, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &reqs[1]);
        MPI_Waitall(2, reqs, MPI_STATUSES_IGNORE);
        msg++;
        std::cout << "Rank 1 processed " << msg << "\n";
    }

    MPI_Finalize();
}