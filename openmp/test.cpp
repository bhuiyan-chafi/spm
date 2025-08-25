#include <iostream>
#include <omp.h>
using namespace std;
int main()
{
#pragma omp parallel
    {
        int id = omp_get_thread_num();
        int number_of_threads = omp_get_num_threads();
        cout << "Hello from thread " << id << " of " << number_of_threads << endl;
    }
    return 0;
}
