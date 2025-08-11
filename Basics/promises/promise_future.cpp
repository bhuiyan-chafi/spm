#include <iostream>
#include <thread>
#include <future>
#include <cstdint>

using namespace std;

void producer(promise<uint16_t> thread_promise, uint16_t passed_value)
{
    cout << "Producer is preparing the value.... " << endl;
    // pass the value to the future
    thread_promise.set_value(passed_value);
}

void consumer(future<uint16_t> thread_future)
{
    cout << "Consumer is waiting for the value from the producer..." << endl;
    uint16_t value = thread_future.get();
    cout << "Passed value from the producer, received by the consumer: " << value << endl;
}

int main()
{
    // define the promise and future
    // the data type *uint16_t* defines that we will handle int values only
    promise<uint16_t> thread_promise;
    // define that this future will read from this promise
    future<uint16_t> thread_future = thread_promise.get_future();

    // define the threads
    // we move them because copying will throw compiling error(explained in readme.md)
    thread promise_thread(producer, move(thread_promise), 100);
    thread future_thread(consumer, move(thread_future));
    promise_thread.join();
    future_thread.join();
    return 0;
}
