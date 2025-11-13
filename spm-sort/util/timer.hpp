#pragma once
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <cstdio>

//
// === TimerClass ============================================================
// - start()/stop() can be called multiple times (accumulates).
// - result() auto-picks unit (µs/ms/s/min).
// - elapsed_ns() returns total as nanoseconds.
//
class TimerClass
{
public:
    using clock = std::chrono::steady_clock;
    using nanosec = std::chrono::nanoseconds;

    void start()
    {
        if (!running_)
        {
            t0_ = clock::now();
            running_ = true;
        }
    }
    void stop()
    {
        if (running_)
        {
            total_ += std::chrono::duration_cast<nanosec>(clock::now() - t0_);
            running_ = false;
        }
    }
    void reset()
    {
        total_ = nanosec::zero();
        running_ = false;
    }

    nanosec elapsed_ns() const
    {
        if (running_)
            return total_ + std::chrono::duration_cast<nanosec>(clock::now() - t0_);
        return total_;
    }

    std::string result(int precision = 3) const
    {
        const long long ns = elapsed_ns().count();
        return humanize_ns(ns, precision);
    }

    // Reusable formatter
    static std::string humanize_ns(long long ns, int precision = 3)
    {
        double value;
        const char *unit;
        if (ns < 1'000)
        {
            // Nanoseconds (< 1 microsecond)
            value = static_cast<double>(ns);
            unit = "ns";
        }
        else if (ns < 1'000'000)
        {
            // Microseconds (< 1 millisecond)
            value = ns / 1'000.0;
            unit = "µs";
        }
        else if (ns < 1'000'000'000)
        {
            // Milliseconds (< 1 second)
            value = ns / 1'000'000.0;
            unit = "ms";
        }
        else if (ns < 60'000'000'000)
        {
            // Seconds (< 1 minute)
            value = ns / 1'000'000'000.0;
            unit = "s";
        }
        else
        {
            // Minutes
            value = ns / 60'000'000'000.0;
            unit = "min";
        }
        char buf[64], fmt[16];
        std::snprintf(fmt, sizeof(fmt), "%%.%df %%s", precision);
        std::snprintf(buf, sizeof(buf), fmt, value, unit);
        return std::string(buf);
    }

private:
    clock::time_point t0_{};
    nanosec total_{0};
    bool running_{false};
};

//
// === TimerScope (RAII wrapper) ============================================
// Starts on construction, stops on destruction.
//
struct TimerScope
{
    explicit TimerScope(TimerClass &t) : timer(t) { timer.start(); }
    ~TimerScope() { timer.stop(); }
    TimerClass &timer;
};

//
// === Timings (shared aggregator for workers) ==============================
// Thread-safe accumulator for a farm with W workers.
// - Each worker calls publish(idx, ns) once (e.g., in svc_end()).
// - total() returns the sum of all workers.
// - per_worker(i) returns one worker’s ns.
//
struct Timings
{
    explicit Timings(std::size_t n)
        : ns_per_worker(n, 0), published(n, false), expected(n) {}

    // Publish a worker’s final elapsed time in ns (call once per worker).
    void publish(std::size_t worker_idx, long long ns)
    {
        std::scoped_lock lk(m);
        if (worker_idx >= ns_per_worker.size())
            return;
        if (!published[worker_idx])
        {
            ns_per_worker[worker_idx] = ns;
            published[worker_idx] = true;
            total_ns += ns;
            ++seen;
        }
    }

    long long total() const
    {
        std::scoped_lock lk(m);
        return total_ns;
    }

    long long per_worker(std::size_t i) const
    {
        std::scoped_lock lk(m);
        return (i < ns_per_worker.size()) ? ns_per_worker[i] : 0LL;
    }

    bool all_published() const
    {
        std::scoped_lock lk(m);
        return seen == expected;
    }

    std::string total_str(int precision = 3) const
    {
        return TimerClass::humanize_ns(total(), precision);
    }

private:
    mutable std::mutex m;
    std::vector<long long> ns_per_worker;
    std::vector<bool> published;
    long long total_ns{0};
    std::size_t seen{0};
    std::size_t expected{0};
};