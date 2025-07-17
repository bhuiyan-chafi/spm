#include <iostream>
#include <ctime>
#include <vector>
// constexpr int n = 1024;
// double A[n][n];
// double B[n][n];
// double C[n][n];
int main()
{
    int n = 4096;
    clock_t start, end;
    double cpu_time_used;
    // Define matrices A, B, and C using vectors in C++
    std::vector<std::vector<double>> A(n, std::vector<double>(n));
    std::vector<std::vector<double>> B(n, std::vector<double>(n));
    std::vector<std::vector<double>> C(n, std::vector<double>(n));

    // Start the clock
    start = clock();

    // 3-step nested for loop[try with i,j,k/i,k,j/k,i,j]
    for (int k = 0; k < n; ++k)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
            {
                C[i][j] = A[i][k] * B[k][j];
            }
        }
        std::cout << "iteration i: " << k << "\n";
    }

    // Stop the clock
    end = clock();

    // Calculate the time taken
    cpu_time_used = static_cast<double>(end - start) / CLOCKS_PER_SEC;

    // Print the execution time
    std::cout << "Execution time: " << cpu_time_used << " seconds" << std::endl;

    return 0;
}