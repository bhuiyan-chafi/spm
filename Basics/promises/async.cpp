#include <iostream>
#include <future>
#include <cstdint>
using namespace std;

uint16_t square(uint16_t value) { return value * value; }

int main()
{
    // async doesn't require explicit call of promise and future. It does the job under the hood. If we create a future object and use async, under the hood we are using promise and future.
    // create the future
    future<uint16_t> handler = async(launch::async, square, 10);
    auto result = handler.get();
    cout << "The result got from the future: " << result << endl;
    return 0;
}