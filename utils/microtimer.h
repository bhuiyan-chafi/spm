// microtimer.h
#pragma once

#include <chrono>
#include <iostream>
#include <iomanip>

// record the start time as soon as this header is included
inline const auto __mt_start = std::chrono::high_resolution_clock::now();

struct MicroTimer
{
    ~MicroTimer()
    {
        using namespace std::chrono;
        auto end = high_resolution_clock::now();
        double secs = duration_cast<duration<double>>(end - __mt_start).count();

        std::cout << std::fixed << std::setprecision(6);
        if (secs < 1e-6)
        { // less than 1 µs
            std::cout << "Run time: " << secs * 1e9 << " ns\n";
        }
        else if (secs < 1e-3)
        { // less than 1 ms
            std::cout << "Run time: " << secs * 1e6 << " µs\n";
        }
        else if (secs < 1.0)
        { // less than 1 s
            std::cout << "Run time: " << secs * 1e3 << " ms\n";
        }
        else
        {
            std::cout << "Run time: " << secs << " s\n";
        }
    }
};

// inline ensures one definition across all translation units
inline MicroTimer __microtimer_instance;
