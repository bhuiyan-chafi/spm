// in this test we will declare a number of threads from the command line. The goal is to calculate the fibonacci numbers from 0,1 till number_of_threads by different threads and store them in a vector. So, each of these threads will take it's range and calculate the fibonacci number simultaneously.

#include <iostream>
#include <thread>
#include <cstdint>
#include <vector>

// we know that we will pass integers only this is why we are avoiding template and auto [tried  but failed]
// the reason we have to use the template is: we will pass the result vector to store the number after that calculation and that vector is not an integer. Now we can implicityly initialize the vector type here but it's far more easier with template. Also, we are passing the result as reference, so, we don't need any specific return type.
template <typename value_t>
void fibonacciCalculator(value_t limit, value_t &fib_number)
{
    value_t first_number = 0, second_number = 1;
    for (uint64_t i = 0; i < limit; i++)
    {
        const uint64_t tempporary = first_number;
        first_number = second_number;
        second_number += tempporary;
    }
    fib_number = first_number;
}

using namespace std;
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Expecting exactly 2 paramemters[program_name and one integer]" << endl;
        return EXIT_FAILURE;
    }
    const uint64_t limit = static_cast<uint64_t>(stoi(argv[1]));
    // define a vector that will hold each number of the fibonacci position(i.e: fib[10] will hold the 10th fibonacci number)
    vector<uint64_t> fibonacci_numbers(limit, 0);
    cout << "Size of the Vector(should be same as the parameter you passed): " << limit << endl;
    // create another vector that will hold the number of threads because we will have as much as the number of the fibonacci positions
    vector<thread> threads;
    cout << "Size of the Current THREAD VECTOR: " << threads.size() << endl;
    cout << "Capacity of the Current THREAD VECTOR: " << threads.capacity() << endl;
    // let's reserve a capacity at least the size of <limit>
    threads.reserve(limit);
    cout << "Capacity after reserving(size:limit) of the Current THREAD VECTOR: " << threads.capacity() << endl;
    // now we will call each threads in a loop and assign the fibonacci calculator
    for (uint64_t thread_number = 0; thread_number < limit; thread_number++)
    {
        // why we cannot call the *fibonacciCalculator* functional in a typical way inside the thread? It's because if we do so, the function gets executed immediately from the main thread which is not our goal. This is why we use lambda function which works as a thread argument here.
        threads.emplace_back([thread_number, &fibonacci_numbers]()
                             { fibonacciCalculator(thread_number, fibonacci_numbers[thread_number]); });
    }
    // join the threads
    for (auto &thread : threads)
    {
        thread.join();
    }
    for (const auto &fibonacci_number : fibonacci_numbers)
    {
        cout << "Current Number: " << fibonacci_number << endl;
    }

    return 0;
}