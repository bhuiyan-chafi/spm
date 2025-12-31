#include <mpi.h>
#include <iostream>
using namespace std;

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0)
    {
        int number = 42;
        MPI_Send(&number, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        cout << "Process 0 sent number " << number << " to process 1\n";
    }
    else if (rank == 1)
    {
        int received;
        MPI_Recv(&received, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        cout << "Process 1 received number " << received << " from process 0\n";
    }

    MPI_Finalize();
    return 0;
}