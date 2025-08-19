#include <iostream>
#include <thread>
#include <cstdint>
#include "../include/threadPool.hpp"
#include "../../utils/microtimer.h"
using namespace std;
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cerr << "Expecting exactly 3 paramemters[program_name, int(number of worker, uint_16(for number of tasks)]" << endl;
        return EXIT_FAILURE;
    }
    // function to calculate the square of the product
    auto square = [](const uint64_t x)
    {
        return x * x;
    };
    int workers = static_cast<int>(stoi(argv[1]));                   // conversion from string to int
    uint64_t number_of_tasks = static_cast<uint64_t>(stoi(argv[2])); // conversion from string to int
    ThreadPool threadPool(workers);                                  // since my machine has 4 physical threads, I will test with this
    vector<future<uint64_t>> results;                                // creating the queue of task,we can use async as well
    for (uint64_t i = 0; i < number_of_tasks; i++)
    {
        results.emplace_back(threadPool.enqueue(square, i));
    }
    // retrieve the values from the future after thread execution
    for (uint64_t task = 0; task < results.size(); task++)
    {
        cout << "Task[" << task << "]" << " | Result[" << results[task].get() << "]" << endl;
    }

    return 0;
}