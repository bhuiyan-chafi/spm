#include <iostream>
#include <thread>
#include <chrono>
#include "../../../utils/microtimer.h" // include the microtimer header
using namespace std;

void callableForThread1()
{
    cout << "Thread 1 is running" << endl;
    // let's simulate some work
    this_thread::sleep_for(chrono::seconds(5));
}

void callableForThread2()
{
    cout << "Thread 2 is running" << endl;
    // let's simulate some work
    this_thread::sleep_for(chrono::seconds(7));
}

int main()
{
    cout << "Starting threads..." << endl;
    thread thread1(callableForThread1);
    thread thread2(callableForThread2);
    cout << "Threads started, now waiting for them to finish..." << endl;
    thread1.join();
    cout << "Thread 1 finished" << endl;
    thread2.join();
    cout << "Thread 2 finished" << endl;
    cout << "All threads have finished execution." << endl;
    return 0;
}