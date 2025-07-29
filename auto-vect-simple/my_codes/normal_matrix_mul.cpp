#include <iostream>
#include <ctime>
#include <vector>
using namespace std;

void matrix_add(double *A, double *B, double *C, size_t rows, size_t cols)
{
    // we simple declared pointers for the vectors A, B, and C becase std::vector.data() returns the pointer to the first element of the vector.
    for (size_t i = 0; i < rows * cols; ++i)
    {
        C[i] += A[i] + B[i];
    }
}

int main()
{
    clock_t start, end;
    double cpu_time_used;
    const size_t rows = 1000000; // 1 million rows
    const size_t cols = 100;
    start = clock();
    // Allocate separate blocks for matrices A and B to remove aliasing.
    vector<double> A(rows * cols);
    vector<double> B(rows * cols);
    vector<double> C(rows * cols);

    // Initialize matrices A and B
    for (size_t i = 0; i < rows * cols; ++i)
    {
        A[i] = i;
        B[i] = i;
        C[i] = i * 0.1f; // Initialize C to zero
    }
    // if you want to see how the vector.data() works, you can print the first element of the vector C.
    // double *A_ptr = A.data();
    // cout << "A_ptr = " << *A_ptr << endl;
    // Perform matrix addition
    matrix_add(A.data(), B.data(), C.data(), rows, cols);
    end = clock();
    // print the first element of C to avoid dead-code elimination.
    cout << "C[10] = " << C[10] << endl;
    cpu_time_used = double(end - start) / CLOCKS_PER_SEC;
    cout << "Time taken: " << cpu_time_used << " seconds" << endl;
    return 0;
}