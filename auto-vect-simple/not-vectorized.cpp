#include <vector>
#include <iostream>

// Perform element-wise addition: A[i][j] += B[i][j]
void inline matrix_add(float *__restrict__ A, float *__restrict__ B, int rows, int cols)
{
#if 1
    for (int i = 0; i < rows; ++i)
    {
#pragma GCC ivdep
        for (int j = 0; j < cols; ++j)
        {
            int idx = i * cols + j;
            A[idx] += B[idx];
        }
    }
#endif
    int n = rows * cols;
    for (int i = 0; i < n; ++i)
    {
        A[i] += B[i];
    }
}

int main()
{
    const int rows = 100;
    const int cols = 50;
    // Allocate separate blocks for matrices A and B to remove aliasing.
    std::vector<float> A(rows * cols);
    std::vector<float> B(rows * cols);

    for (int i = 0; i < rows * cols; ++i)
    {
        A[i] = i * 0.1;
        B[i] = i * 0.2;
    }
    matrix_add(A.data(), B.data(), rows, cols);

    // Print one result to avoid dead-code elimination.
    std::cout << "A[0] = " << A[0] << std::endl;

    return 0;
}
