// microtimer.h
#pragma once

#include <chrono>
#include <iostream>
#include <iomanip>

class MicroTimer
{
private:
    using clock = std::chrono::high_resolution_clock;
    using time_point = std::chrono::time_point<clock>;

    time_point start_time;
    bool running = false;

public:
    // Start measuring time
    void start()
    {
        start_time = clock::now();
        running = true;
    }

    // Stop and print the elapsed time
    void end()
    {
        if (!running)
        {
            std::cerr << "MicroTimer error: end() called before start()\n";
            return;
        }
        auto end_time = clock::now();
        double secs = std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time).count();

        std::cout << std::fixed << std::setprecision(6);
        if (secs < 1e-6)
        {
            std::cout << "Run time: " << secs * 1e9 << " ns\n";
        }
        else if (secs < 1e-3)
        {
            std::cout << "Run time: " << secs * 1e6 << " µs\n";
        }
        else if (secs < 1.0)
        {
            std::cout << "Run time: " << secs * 1e3 << " ms\n";
        }
        else
        {
            std::cout << "Run time: " << secs << " s\n";
        }

        running = false;
    }
};
