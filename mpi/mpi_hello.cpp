#include <mpi.h>
#include <iostream>
using namespace std;

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv); // Initialize MPI environment

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // Get process ID (rank)
    MPI_Comm_size(MPI_COMM_WORLD, &size); // Get total number of processes

    cout << "Hello from process " << rank << " of " << size << endl;

    MPI_Finalize(); // Clean up MPI environment
    return 0;
}