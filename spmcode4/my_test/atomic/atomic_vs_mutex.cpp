#include <iostream>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <vector>
#include <thread>
#include "../../../utils/micro_smart_clock.h"
using namespace std;

int main(int argc, char *argv[])
{
    MicroTimer timer;
    if (argc != 3)
    {
        cerr << "Expecting exactly 3 paramemters[program_name, int(number of threads), uint_16(for iteration)]" << endl;
        return EXIT_FAILURE;
    }
    // receive number of threads and range of iteration
    const uint64_t number_of_threads = static_cast<uint64_t>(stoull(argv[1]));
    const uint64_t iteration_range = static_cast<uint64_t>(stoull(argv[2]));
    // declare a vector for our threads
    vector<thread> threads;
    // mutex lock
    mutex mutex_lock;
    // function to perform mutex lock counter
    auto mutex_lock_count = [&](uint64_t &counter, const auto &index)
    {
        for (uint64_t i = index; i < iteration_range; i += number_of_threads)
        {
            lock_guard<mutex> lock_guard(mutex_lock);
            counter++; // why (*counter) because ++ has higher precedence than *, if we do *counter++ it will increment the memory location not the value inside the memory location
        }
    };
    // perform the atmoic counter without lock
    auto atomic_lock_count = [&](atomic<uint64_t> &counter, const auto &index)
    {
        for (uint64_t i = index; i < iteration_range; i += number_of_threads)
        {
            counter++;
        }
    };
    // start the timer for first experiment
    timer.start();
    // the mutex counter variable
    uint64_t mutex_lock_counter = 0;
    // clear the threads for safety
    threads.clear();
    for (uint64_t id = 0; id < number_of_threads; id++)
    {
        threads.emplace_back(mutex_lock_count, ref(mutex_lock_counter), ref(id));
    }
    for (auto &thread : threads)
    {
        thread.join();
    }
    timer.end();
    //-------first experiment(mutex) ends
    threads.clear();
    // starting the timer for second experiment
    timer.start();
    atomic<uint64_t> atomic_counter(0);
    for (uint64_t id = 0; id < number_of_threads; id++)
    {
        threads.emplace_back(atomic_lock_count, ref(atomic_counter), ref(id));
    }
    for (auto &thread : threads)
    {
        thread.join();
    }
    timer.end();
    //----second experiment(atomic) ends and printing the results
    cout << "Mutex Counter: " << mutex_lock_counter << " | Atomic Counter: " << atomic_counter << endl;
}