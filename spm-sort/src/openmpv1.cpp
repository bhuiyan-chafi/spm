/**
 * @file openmpv1.cpp
 * @brief the first version I built with OPENMP. Has several issues, but let me point some major ones:
 * - no distribution
 * - idle workers
 * - memory bloat
 * - no parallelism to be honest except OPENMP pragmas
 * - static scheduling of tasks
 * @file ./discussions/openmpv1.md
 * - contains a brief summary with codes to prove some of the points
 * - please read them and run the tests if possible
 */
#include "main.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace omp_sort
{
    namespace
    {
        using Items = std::vector<Item>;

        struct PendingRecord
        {
            Item item;
            uint64_t bytes;
        };

        struct Task
        {
            std::unique_ptr<Items> items;
            bool spill;
            size_t segment_id;
            size_t slice_index;
        };

        enum class ResultKind
        {
            InMemBatch,
            RunPath
        };

        struct TaskResult
        {
            ResultKind kind{ResultKind::InMemBatch};
            bool spill{false};
            size_t segment_id{0};
            size_t slice_index{0};
            std::unique_ptr<Items> items;
            std::string path;
        };

        std::vector<std::pair<size_t, size_t>>
        slice_ranges(size_t n, size_t parts)
        {
            if (parts == 0)
                parts = 1;
            std::vector<std::pair<size_t, size_t>> ranges;
            ranges.reserve(parts);
            for (size_t i = 0; i < parts; ++i)
            {
                size_t L = (i * n) / parts;
                size_t R = ((i + 1) * n) / parts;
                if (L < R)
                    ranges.emplace_back(L, R);
            }
            if (ranges.empty())
                ranges.emplace_back(0, n);
            return ranges;
        }

        uint64_t record_size_bytes(const Item &item)
        {
            return sizeof(uint64_t) + sizeof(uint32_t) + item.payload.size();
        }
        // create tasks for the worker
        std::vector<Task> emit_tasks(size_t workers, bool &saw_spill)
        {
            std::ifstream in(DATA_INPUT, std::ios::binary);
            if (!in)
            {
                // spdlog::error("Emitter: cannot open input {}", DATA_INPUT);
                throw std::runtime_error("Input stream error");
            }

            std::vector<Task> tasks;
            std::optional<PendingRecord> carry;
            size_t segment_id = 0;
            saw_spill = false;

            while (true)
            {
                auto segment = std::make_unique<Items>();
                uint64_t accumulated = 0ULL;
                bool eof_reached = false;
                bool overflow = false;

                if (carry)
                {
                    accumulated += carry->bytes;
                    segment->push_back(std::move(carry->item));
                    carry.reset();
                }
                // read -> check -> carry or push
                while (true)
                {
                    uint64_t key;
                    CompactPayload payload;
                    if (!read_record(in, key, payload))
                    {
                        eof_reached = true;
                        break;
                    }
                    Item next{key, std::move(payload)};
                    const uint64_t next_size = record_size_bytes(next);
                    // carry
                    if (!segment->empty() && accumulated + next_size > MEMORY_CAP)
                    {
                        carry = PendingRecord{Item{next.key, std::move(next.payload)}, next_size};
                        overflow = true;
                        break;
                    }
                    accumulated += next_size;
                    // push
                    segment->push_back(std::move(next));
                }

                if (segment->empty())
                {
                    // means one segment is complete
                    if (overflow)
                        continue;
                    // means EOF
                    break;
                }
                // decide if we have to write this segment -> OOC operation
                const bool segment_spill = saw_spill || overflow || carry.has_value();
                saw_spill = saw_spill || segment_spill;
                // now we have one segment = MEMORY_CAP
                // distribute them among workers
                auto ranges = slice_ranges(segment->size(), workers);
                for (size_t i = 0; i < ranges.size(); ++i)
                {
                    auto [L, R] = ranges[i];
                    auto slice = std::make_unique<Items>();
                    slice->reserve(R - L);
                    for (size_t j = L; j < R; ++j)
                        slice->push_back(std::move((*segment)[j]));
                    tasks.push_back(Task{std::move(slice), segment_spill, segment_id, i});
                }
                ++segment_id;

                if (eof_reached && !carry.has_value())
                    break;
            }

            return tasks;
        }
        // the distributor and sorter -> worker
        std::vector<TaskResult> process_tasks(std::vector<Task> tasks, size_t threads, bool saw_spill)
        {
            if (tasks.empty())
                return {};

            if (saw_spill)
            {
                std::error_code ec;
                std::filesystem::create_directories(DATA_TMP_DIR, ec);
                if (ec)
                {
                    // spdlog::warn("Could not ensure tmp dir {} exists: {}", DATA_TMP_DIR, ec.message());
                }
            }

            std::vector<TaskResult> results(tasks.size());
            static std::atomic<uint64_t> run_id{0};

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic) num_threads(static_cast<int>(threads))
#endif
            for (size_t i = 0; i < tasks.size(); ++i)
            {
                auto &task = tasks[i];
                auto *local_items = task.items.get();
                std::sort(local_items->begin(), local_items->end(),
                          [](const Item &a, const Item &b)
                          { return a.key < b.key; });

                TaskResult result;
                result.spill = task.spill;
                result.segment_id = task.segment_id;
                result.slice_index = task.slice_index;
                if (!task.spill)
                {
                    result.kind = ResultKind::InMemBatch;
                    result.items = std::move(task.items);
                }
                else
                {
                    result.kind = ResultKind::RunPath;
                    const auto id = run_id.fetch_add(1, std::memory_order_relaxed);
                    result.path = DATA_TMP_DIR + "run_" + std::to_string(id) + ".bin";
                    write_temp_slice(result.path, *local_items);
                }
                results[i] = std::move(result);
            }

            return results;
        }
        // all the TaskResults are collected and performed final merge and write
        void finalize_in_memory(std::vector<TaskResult> &results)
        {
            std::ofstream out(DATA_OUTPUT, std::ios::binary);
            if (!out)
                throw std::runtime_error("Collector(inmem): cannot open output");

            struct CurrentItem
            {
                size_t batch_index;
                size_t item_index;
                uint64_t key;
            };
            struct Compare
            {
                bool operator()(const CurrentItem &a, const CurrentItem &b) const
                {
                    return a.key > b.key;
                }
            };

            std::vector<std::unique_ptr<Items>> batches;
            batches.reserve(results.size());
            for (auto &res : results)
                if (res.kind == ResultKind::InMemBatch && res.items && !res.items->empty())
                    batches.push_back(std::move(res.items));

            std::priority_queue<CurrentItem, std::vector<CurrentItem>, Compare> heap;
            for (size_t b = 0; b < batches.size(); ++b)
                heap.push(CurrentItem{b, 0, (*batches[b])[0].key});

            size_t written = 0;
            while (!heap.empty())
            {
                auto current = heap.top();
                heap.pop();
                const Item &item = (*batches[current.batch_index])[current.item_index];
                write_record(out, item.key, item.payload);
                ++written;
                const size_t next_index = current.item_index + 1;
                if (next_index < batches[current.batch_index]->size())
                    heap.push(CurrentItem{current.batch_index, next_index, (*batches[current.batch_index])[next_index].key});
            }

            // spdlog::info("Collector(inmem): wrote {} records -> {}", written, DATA_OUTPUT);
        }
        // write one segment while other segments are getting processed
        void finalize_spill(std::vector<TaskResult> &results)
        {
            std::vector<std::string> run_paths;
            run_paths.reserve(results.size());
            for (auto &res : results)
                if (res.kind == ResultKind::RunPath && !res.path.empty())
                    run_paths.push_back(std::move(res.path));

            if (run_paths.empty())
            {
                // spdlog::warn("Collector(ooc): no runs produced");
                std::ofstream(DATA_OUTPUT, std::ios::binary);
                return;
            }

            std::ofstream out(DATA_OUTPUT, std::ios::binary);
            if (!out)
                throw std::runtime_error("Collector(ooc): cannot open output");

            std::vector<std::unique_ptr<TempReader>> readers;
            readers.reserve(run_paths.size());
            for (const auto &path : run_paths)
                readers.push_back(std::make_unique<TempReader>(path));

            struct HeapNode
            {
                uint64_t key;
                size_t run_index;
            };
            struct Compare
            {
                bool operator()(const HeapNode &a, const HeapNode &b) const
                {
                    return a.key > b.key;
                }
            };

            std::priority_queue<HeapNode, std::vector<HeapNode>, Compare> heap;
            for (size_t r = 0; r < readers.size(); ++r)
                if (!readers[r]->eof)
                    heap.push(HeapNode{readers[r]->key, r});

            size_t written = 0;
            while (!heap.empty())
            {
                auto current = heap.top();
                heap.pop();
                auto &reader = *readers[current.run_index];
                write_record(out, reader.key, reader.payload);
                ++written;
                reader.advance();
                if (!reader.eof)
                    heap.push(HeapNode{reader.key, current.run_index});
            }
            // spdlog::info("Collector(ooc): wrote {} records -> {}", written, DATA_OUTPUT);

            for (const auto &path : run_paths)
            {
                std::error_code ec;
                std::filesystem::remove(path, ec);
            }
        }

        void run_pipeline(size_t threads)
        {
            TimerClass emitter_time;
            TimerClass worker_time;
            TimerClass collector_time;

            bool saw_spill = false;
            std::vector<Task> tasks;
            {
                TimerScope ts(emitter_time);
                tasks = emit_tasks(threads ? threads : 1, saw_spill);
            }
            // spdlog::info("Emitter: produced {} tasks, spill={}", tasks.size(), saw_spill ? "true" : "false");

            std::vector<TaskResult> results;
            {
                TimerScope ts(worker_time);
                results = process_tasks(std::move(tasks), threads ? threads : 1, saw_spill);
            }
            // spdlog::info("Worker: processed {} tasks", results.size());
            // if we surpassed MEMORY_CAP or Within MEMORY
            {
                TimerScope ts(collector_time);
                if (saw_spill)
                {
                    report.METHOD = "MEMORY_OOC";
                    finalize_spill(results);
                }
                else
                {
                    report.METHOD = "IN_MEMORY";
                    finalize_in_memory(results);
                }
            }

            report.WORKING_TIME = worker_time.result();
            // spdlog::info("[Timer] Emitter: {}", emitter_time.result());
            // spdlog::info("[Timer] Worker : {}", worker_time.result());
            // spdlog::info("[Timer] Collector: {}", collector_time.result());
        }
    } // namespace

    void execute(size_t threads)
    {
        run_pipeline(threads);
    }
} // namespace omp_sort

int main(int argc, char **argv)
{
    parse_cli_and_set(argc, argv);

    size_t threads = (WORKERS > 0) ? static_cast<size_t>(WORKERS) : 0;
#if defined(_OPENMP)
    if (threads == 0)
        threads = static_cast<size_t>(omp_get_max_threads());
#endif
    if (threads == 0)
        threads = 1;

    // spdlog::info("==> THREADS configured: {} <==", threads);

    TimerClass total_time;
    // spdlog::info("==> Calculating INPUT_SIZE <==");
    // const uint64_t stream_size = estimate_stream_size();
    // spdlog::info("==> INPUT SIZE: {} bytes (~{} GiB) <==", stream_size, stream_size / (1024.0 * 1024.0 * 1024.0));

    try
    {
        TimerScope total_scope(total_time);
        omp_sort::execute(threads);
    }
    catch (const std::exception &error)
    {
        spdlog::error("==> X Operation aborted due to: {} X <==", error.what());
        return EXIT_FAILURE;
    }

    // spdlog::info("->[Timer] : Total OMP Sorting Time -> {}", total_time.result());
    // spdlog::info("==> Completed: Merge Sort, output -> {} <==", DATA_OUTPUT);
    report.TOTAL_TIME = total_time.result();
    spdlog::info("M: {} | R: {} | PS: {} | W: {} | DC:{} | WT: {} | TT: {}", report.METHOD, report.RECORDS, report.PAYLOAD_SIZE, report.WORKERS, "NONE", report.WORKING_TIME, report.TOTAL_TIME);
    return EXIT_SUCCESS;
}