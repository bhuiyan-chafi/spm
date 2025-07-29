#include <iostream>
using namespace std;

int main()
{
    clock_t start, end; // c++ doesn't give us seconds directly. It counts the ticks after the start and end.
    double cpu_time_used;
    start = clock();
    for (int i = 0; i < 5000000; i++)
    {
        //
    }
    end = clock();
    cpu_time_used = static_cast<double>(end - start) / CLOCKS_PER_SEC; // finally we have to divide how many ticks in one second to retrieve seconds. And the conversion is to have fractional seconds.
    cout << "Execution time: " << cpu_time_used << " seconds" << endl;
    return 0;
}