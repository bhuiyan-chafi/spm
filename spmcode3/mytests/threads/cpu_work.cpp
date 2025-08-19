#include <iostream>
#include <thread>
#include <chrono>
#include <cstdint>
#include "../../../utils/microtimer.h" // include the microtimer header
using namespace std;

void callableForThread1()
{
    cout << "Thread 1 is running" << endl;
    // let's simulate some work
    for (uint64_t i = 0; i < 1000000000; ++i)
    {
        // Simulating CPU work
        volatile uint64_t temp = i * i; // Prevent optimization
    }
}

void callableForThread2()
{
    cout << "Thread 2 is running" << endl;
    // let's simulate some work
    for (uint64_t i = 0; i < 10000000000; ++i)
    {
        // Simulating CPU work
        volatile uint64_t temp = i * i; // Prevent optimization
    }
}

void callableForThread3()
{
    cout << "Thread 3 is running" << endl;
    // let's simulate some work
    for (uint64_t i = 0; i < 100000000000; ++i)
    {
        // Simulating CPU work
        volatile uint64_t temp = i * i; // Prevent optimization
    }
}

void callableForThread4()
{
    cout << "Thread 2 is running" << endl;
    // let's simulate some work
    for (uint64_t i = 0; i < 1000000000000; ++i)
    {
        // Simulating CPU work
        volatile uint64_t temp = i * i; // Prevent optimization
    }
}

int main()
{
    cout << "Starting threads..." << endl;
    thread thread1(callableForThread1);
    thread thread2(callableForThread2);
    thread thread3(callableForThread3);
    thread thread4(callableForThread4);
    cout << "Threads started, now waiting for them to finish..." << endl;
    thread1.join();
    cout << "Thread 1 finished" << endl;
    thread2.join();
    cout << "Thread 2 finished" << endl;
    thread3.join();
    cout << "Thread 3 finished" << endl;
    thread4.join();
    cout << "Thread 4 finished" << endl;
    cout << "All threads have finished execution." << endl;
    return 0;
    // but what happens when we want to pass paramemters to the thread functions?
}