#include <iostream>
#include <thread>
#include <cstdint>
#include <functional>

using namespace std;

uint64_t threadWithParameter(uint64_t limit, uint64_t &result)
{
    return result = limit * limit;
}

int main()
{
    uint64_t result = 0, value = 50;

    thread simpleThread(threadWithParameter, value, std::ref(result));
    simpleThread.join();
    cout << "Result: " << result << endl;
    return 0;
}
