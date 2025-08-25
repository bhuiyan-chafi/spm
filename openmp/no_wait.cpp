#include <iostream>
#include <cstdint>
#include <omp.h>
#include <vector>
#include <atomic>
#include "../utils/microtimer.h"
using namespace std;
// we have 4 threads in our physical machine. So, we are not adding any for this test
int main()
{
    /**
     * We will do two tests by running two loops. The first loop will be defined as nowait and the second one will be a normal OpenMP for loop
     */
    // test with small number(100) and uncomment the couts inside the #pragma for loops to check the interleaving printing which proves the threads don't wait for the first loop to end
    const uint64_t iterations = 100'000'000;
    /**
     * to solve the data-race condition
     * atmoic<uint64_t> sum_one{0};
     * atmoic<uint64_t> sum_two{0};
     */
    uint64_t sum_one = 0;
    uint64_t sum_two = 0;
    vector<uint64_t> numbers(iterations, 1);
#pragma omp parallel
    {
#pragma omp for nowait

        for (uint64_t index = 0; index < iterations; index++)
        {
            sum_one += numbers[index];
            // uncomment if testing with small number
            // cout << "Currently Processing no-wait loop..." << endl;
        }

#pragma omp for

        for (uint64_t index = 0; index < iterations; index++)
        {
            sum_two += numbers[index];
            // uncomment if testing with small number
            // cout << "Currently Processing normal loop..." << endl;
        }
    }
    cout << "Final Result of SUM-1: " << sum_one << " | SUM-2: " << sum_two << endl;
    return 0;
}
/**
 * But what is the issue, here?
 *
 * We have more than 1 thread and two variables which are shared among threads. For example 4 threads in my machine are trying to read and write the same sum_one and sum_two variable. This is a data-race situation where we lost half of the iteration and thats why the sum is half of the iteration.
 */