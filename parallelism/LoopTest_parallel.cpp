#include <iostream>
#include <ctime>
#include <vector>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
using namespace std;
constexpr int n = 4096;
double A[n][n];
double B[n][n];
double C[n][n];
int main()
{
    // int n = 4096;
    clock_t start, end;
    double cpu_time_used;
    // Define matrices A, B, and C using vectors in C++
    // std::vector<std::vector<double>> A(n, std::vector<double>(n));
    // std::vector<std::vector<double>> B(n, std::vector<double>(n));
    // std::vector<std::vector<double>> C(n, std::vector<double>(n));
    int workers = __cilkrts_get_nworkers(); // Number of workers for Cilk
    // Start the clock
    start = clock();

    // 3-step nested for loop[try with i,j,k/i,k,j/k,i,j]
    cilk_for(int k = 0; k < n; ++k)
    {
        for (int j = 0; j < n; ++j)
        {
            cilk_for(int i = 0; i < n; ++i)
            {
                C[i][j] = A[i][k] * B[k][j];
            }
        }
    }

    // Stop the clock
    end = clock();

    // Calculate the time taken
    cpu_time_used = static_cast<double>(end - start) / CLOCKS_PER_SEC;

    // Print the execution time
    cout << "Execution time: " << cpu_time_used << " seconds" << endl;
    cout << "Number of workers: " << workers << endl;

    return 0;
}