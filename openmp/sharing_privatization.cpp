#include <iostream>
#include <omp.h>
using namespace std;

int main()
{
    int a = 0, b = 1, c = 2, d = 3;
/**
 * Let's check how each thread handles the variables according to their scope, num_threads = 4 by default
 * we couldn't check lastprivate(x) here because it's only applicable inside a for loop
 */
#pragma omp parallel private(a) shared(b) firstprivate(c)
    {
        a++;
        b++;
        c++;
        d++;
        printf("From thread %d a(private)=%d b(shared)=%d c(firstprivate)=%d d(default)=%d\n", omp_get_thread_num(), a, b, c, d);
    }
    printf("Final Values a=%d b=%d c=%d d=%d\n", a, b, c, d);
    return 0;
}