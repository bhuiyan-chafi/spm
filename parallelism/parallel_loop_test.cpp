#include <cilk/cilk.h>
#include <iostream>
using namespace std;
int main()
{
    double start = 0.0;
    cilk_for(int i = 0; i < 4096; ++i)
            cout
        << "Hello from iteration " << i << "\n";
    return 0;
}
