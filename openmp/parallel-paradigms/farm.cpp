#include <iostream>
#include <omp.h>
#include <thread>
#include <chrono>

using namespace std;

void heavy_worker(int id, uint64_t limit)
{
    volatile uint64_t sum = 0;
    for (uint64_t i = 0; i < limit; ++i)
        sum += i;
    cout << "Worker " << id << ": Finished heavy task (limit = " << limit << ")\n";
}

int main()
{
    const int num_tasks = 10;     // Simulate 10 streamed inputs
    const int num_threads = 4;    // Number of workers
    const uint64_t N = 100000000; // Work granularity

    omp_set_num_threads(num_threads);

#pragma omp parallel
    {
#pragma omp single
        {
            for (int i = 0; i < num_tasks; ++i)
            {
#pragma omp task
                {
                    int thread_id = omp_get_thread_num();
                    if (thread_id == 0)
                    {
                        // Simulate heavy task
                        heavy_worker(thread_id, N / 2);
                    }
                    else
                    {
                        // Lighter task
                        heavy_worker(thread_id, N / 8);
                    }
                }
            }
        }
    }

    return 0;
}