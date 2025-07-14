#include <iostream>
#include <ctime>
#include <vector>

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

    // 3-step nested for loop
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            for (int k = 0; k < n; ++k)
            {
                C[i][j] = A[i][k] * B[j][k];
            }
        }
        std::cout << "iteration i: " << i << "\n";
    }

    // Stop the clock
    end = clock();

    // Calculate the time taken
    cpu_time_used = static_cast<double>(end - start) / CLOCKS_PER_SEC;

    // Print the execution time
    std::cout << "Execution time: " << cpu_time_used << " seconds" << std::endl;

    return 0;
}