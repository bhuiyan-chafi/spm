#include <iostream>
#include <vector>
#include <omp.h>
#include <thread>
#include <chrono>
#include <cstdint>
#include "../../utils/microtimer.h"

using namespace std;
using namespace chrono;

const size_t counter_limit = 100'000'000'000;

void f_rm_blur()
{
    size_t result = 0;
    for (size_t index = 0; index < counter_limit; index++)
    {
        result = index;
    }
    cout << result << endl;
}

void f_d_object()
{
    size_t result = 0;
    for (size_t index = 0; index < counter_limit; index++)
    {
        result = index;
    }
    cout << result << endl;
}

void f_m_object()
{
    size_t result = 0;
    for (size_t index = 0; index < counter_limit; index++)
    {
        result = index;
    }
    cout << result << endl;
}

void f_v_object()
{
    size_t result = 0;
    for (size_t index = 0; index < counter_limit; index++)
    {
        result = index;
    }
    cout << result << endl;
}

void f_state_object()
{
    size_t result = 0;
    for (size_t index = 0; index < counter_limit; index++)
    {
        result = index;
    }
    cout << result << endl;
}

void F(int id)
{
    f_rm_blur();
    f_d_object();
    f_m_object();
    f_v_object();
    f_state_object();
}

int main()
{
    const uint32_t number = 10000; // Batch size (>= 4 to utilize all cores)

#pragma omp parallel for
    for (int i = 0; i < number; ++i)
    {
        F(i);
    }

    return 0;
}