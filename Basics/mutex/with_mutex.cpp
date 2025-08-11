#include <iostream>
#include <thread>
#include <cstdint>
#include <mutex>

using namespace std;

// let's define one globally and use everywhere
mutex mutex_lock;

uint16_t counter(uint16_t range, uint16_t &counter_index)
{
    for (uint16_t index = 0; index < range; index++)
    {
        // try to stop the data-race
        mutex_lock.lock();
        ++counter_index;
        cout << "Incremented by: " << this_thread::get_id() << endl;
        cout << "Current value of the COUNTER_INDEX: " << counter_index << endl;
        mutex_lock.unlock();

        // another alternative is to create a scope and use lock_guard<mutex> lock(mutex_variable)
        /**
         * {
             lock_guard<mutex> lock(mutex_lock); // critical section starts
             ++counter_index;                    // shared state protected
         }
         *
         **/
    }
    return counter_index;
}

int main()
{

    uint16_t counter_index = 0;
    // two thread with the same function and a shared value 'counter' to increment
    thread thread1(counter, 1000, ref(counter_index));
    thread thread2(counter, 1000, ref(counter_index));
    thread1.join();
    thread2.join();
    cout << "Final value of the COUNTER: " << counter_index << " | Expected: " << 2000 << endl;
    return 0;
}