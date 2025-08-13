#include <iostream>
#include <vector>
#include <numeric>
#include <chrono>
#include "../utils/microtimer.h"
int main()
{
    const size_t N = 1000000000;
    std::vector<int> data(N, 1);
    long long sum = 0;
#pragma omp parallel for reduction(+ : sum)
    for (size_t i = 0; i < N; ++i)
    {
        sum += data[i];
    }

    std::cout << "Sum = " << sum << "\n";
}