/**
 * memory bloat is fixed for oversized file but it writes too many files, but it's fine otherwise we will write one segment by N workers that will produce race conditions
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

        struct SegmentReader
        {
            std::ifstream in;
            std::optional<PendingRecord> carry;
            bool eof = false;
            bool any_spill = false;

            explicit SegmentReader(const std::string &path) : in(path, std::ios::binary)
            {
                if (!in)
                {
                    spdlog::error("SegmentReader: cannot open input {}", path);
                    throw std::runtime_error("Input stream error");
                }
            }

            std::unique_ptr<Items> read_next_segment()
            {
                if (eof)
                    return nullptr;

                auto segment = std::make_unique<Items>();
                uint64_t accumulated = 0ULL;
                bool overflow = false;

                if (carry)
                {
                    accumulated += carry->bytes;
                    segment->push_back(std::move(carry->item));
                    carry.reset();
                }

                while (true)
                {
                    uint64_t key;
                    std::vector<uint8_t> payload;
                    if (!read_record(in, key, payload))
                    {
                        eof = true;
                        break;
                    }
                    Item next{key, std::move(payload)};
                    const uint64_t next_size = record_size_bytes(next);
                    if (!segment->empty() && accumulated + next_size > MEMORY_CAP)
                    {
                        carry = PendingRecord{Item{next.key, std::move(next.payload)}, next_size};
                        overflow = true;
                        break;
                    }
                    accumulated += next_size;
                    segment->push_back(std::move(next));
                }

                if (segment->empty())
                    return nullptr;

                if (overflow || carry.has_value())
                    any_spill = true;

                return segment;
            }
        };

        std::vector<Task> process_segment(std::unique_ptr<Items> segment, size_t segment_id,
                                          size_t threads, bool is_spill, bool is_final_segment)
        {
            if (!segment || segment->empty())
                return {};

            auto ranges = slice_ranges(segment->size(), threads);
            std::vector<Task> tasks;
            tasks.reserve(ranges.size());

            for (size_t i = 0; i < ranges.size(); ++i)
            {
                auto [L, R] = ranges[i];
                auto slice = std::make_unique<Items>();
                slice->reserve(R - L);
                for (size_t j = L; j < R; ++j)
                    slice->push_back(std::move((*segment)[j]));
                tasks.push_back(Task{std::move(slice), is_spill, segment_id, i});
            }

            // segment destroyed here, memory freed

            if (is_spill)
            {
                std::error_code ec;
                std::filesystem::create_directories(DATA_TMP_DIR, ec);
                if (ec)
                    spdlog::warn("Could not ensure tmp dir {} exists: {}", DATA_TMP_DIR, ec.message());
            }

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

                if (task.spill)
                {
                    const auto id = run_id.fetch_add(1, std::memory_order_relaxed);
                    const std::string path = DATA_TMP_DIR + "run_" + std::to_string(id) + ".bin";
                    write_temp_slice(path, *local_items);
                    task.items.reset(); // Free memory immediately after write
                }
            }

            // Return tasks for collector to process
            return tasks;
        }

        void collect_in_memory(std::vector<Task> &tasks)
        {
            std::ofstream out(DATA_OUTPUT, std::ios::binary);
            if (!out)
                throw std::runtime_error("Cannot open output file for direct write");

            struct HeapItem
            {
                size_t task_idx;
                size_t item_idx;
                uint64_t key;
            };
            struct Compare
            {
                bool operator()(const HeapItem &a, const HeapItem &b) const
                {
                    return a.key > b.key;
                }
            };

            std::priority_queue<HeapItem, std::vector<HeapItem>, Compare> heap;
            for (size_t t = 0; t < tasks.size(); ++t)
                if (tasks[t].items && !tasks[t].items->empty())
                    heap.push(HeapItem{t, 0, (*tasks[t].items)[0].key});

            size_t written = 0;
            while (!heap.empty())
            {
                auto current = heap.top();
                heap.pop();
                const Item &item = (*tasks[current.task_idx].items)[current.item_idx];
                write_record(out, item.key, item.payload);
                ++written;

                const size_t next_idx = current.item_idx + 1;
                if (next_idx < tasks[current.task_idx].items->size())
                    heap.push(HeapItem{current.task_idx, next_idx, (*tasks[current.task_idx].items)[next_idx].key});
            }

            spdlog::info("Collector(inmem): wrote {} records -> {}", written, DATA_OUTPUT);
        }

        std::vector<std::string> collect_to_temp_files(std::vector<Task> &tasks)
        {
            std::vector<std::string> run_paths;
            static std::atomic<uint64_t> run_id{0};

            if (tasks.empty())
                return run_paths;

            // For spill case, paths already collected during worker phase
            bool any_spill = false;
            for (const auto &task : tasks)
            {
                if (task.spill)
                {
                    any_spill = true;
                    break;
                }
            }

            if (any_spill)
            {
                // Collect the paths that were written during worker phase
                for (size_t i = 0; i < tasks.size(); ++i)
                {
                    if (tasks[i].spill)
                    {
                        const auto id = i; // Path was already written with this ID
                        const std::string path = DATA_TMP_DIR + "run_" + std::to_string(id) + ".bin";
                        run_paths.push_back(path);
                    }
                }
            }
            else
            {
                // In-memory multi-segment: merge sorted slices and write to temp file
                const auto id = run_id.fetch_add(1, std::memory_order_relaxed);
                const std::string path = DATA_TMP_DIR + "run_" + std::to_string(id) + ".bin";

                std::ofstream out(path, std::ios::binary);
                if (!out)
                    throw std::runtime_error("Cannot open temp file for segment merge");

                struct HeapItem
                {
                    size_t task_idx;
                    size_t item_idx;
                    uint64_t key;
                };
                struct Compare
                {
                    bool operator()(const HeapItem &a, const HeapItem &b) const
                    {
                        return a.key > b.key;
                    }
                };

                std::priority_queue<HeapItem, std::vector<HeapItem>, Compare> heap;
                for (size_t t = 0; t < tasks.size(); ++t)
                    if (tasks[t].items && !tasks[t].items->empty())
                        heap.push(HeapItem{t, 0, (*tasks[t].items)[0].key});

                while (!heap.empty())
                {
                    auto current = heap.top();
                    heap.pop();
                    const Item &item = (*tasks[current.task_idx].items)[current.item_idx];
                    write_record(out, item.key, item.payload);

                    const size_t next_idx = current.item_idx + 1;
                    if (next_idx < tasks[current.task_idx].items->size())
                        heap.push(HeapItem{current.task_idx, next_idx, (*tasks[current.task_idx].items)[next_idx].key});
                }

                run_paths.push_back(path);
            }

            return run_paths;
        }

        void final_merge(const std::vector<std::string> &run_paths)
        {
            if (run_paths.empty())
            {
                spdlog::warn("final_merge: no runs to merge");
                std::ofstream(DATA_OUTPUT, std::ios::binary);
                return;
            }

            std::ofstream out(DATA_OUTPUT, std::ios::binary);
            if (!out)
                throw std::runtime_error("final_merge: cannot open output");

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
            spdlog::info("final_merge: wrote {} records -> {}", written, DATA_OUTPUT);

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

            SegmentReader reader(DATA_INPUT);
            std::vector<std::string> all_run_paths;
            std::vector<Task> single_segment_tasks;
            size_t segment_id = 0;
            bool is_single_segment = false;

            // Emitter + Worker phase
            while (true)
            {
                std::unique_ptr<Items> segment;
                {
                    TimerScope ts(emitter_time);
                    segment = reader.read_next_segment();
                    if (!segment)
                        break;
                }

                bool is_final = reader.eof;

                spdlog::info("Processing segment {} ({} items, spill={}, final={})",
                             segment_id, segment->size(), reader.any_spill ? "true" : "false",
                             is_final ? "true" : "false");

                std::vector<Task> tasks;
                {
                    TimerScope ts(worker_time);
                    tasks = process_segment(std::move(segment), segment_id,
                                            threads ? threads : 1, reader.any_spill, is_final);
                }

                // Check if this is a single in-memory segment
                if (segment_id == 0 && is_final && !reader.any_spill)
                {
                    is_single_segment = true;
                    single_segment_tasks = std::move(tasks);
                }
                else
                {
                    // Multi-segment: collect to temp files
                    auto run_paths = collect_to_temp_files(tasks);
                    all_run_paths.insert(all_run_paths.end(), run_paths.begin(), run_paths.end());
                }

                ++segment_id;
            }

            spdlog::info("Processed {} segments, produced {} run files", segment_id, all_run_paths.size());

            // Collector phase
            {
                TimerScope ts(collector_time);

                if (is_single_segment)
                {
                    // Single segment: merge in-memory slices directly to output
                    collect_in_memory(single_segment_tasks);
                }
                else if (!all_run_paths.empty())
                {
                    // Multi-segment: final merge of temp files
                    final_merge(all_run_paths);
                }
            }

            spdlog::info("[Timer] Emitter: {}", emitter_time.result());
            spdlog::info("[Timer] Worker : {}", worker_time.result());
            spdlog::info("[Timer] Collector: {}", collector_time.result());
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

    spdlog::info("==> THREADS configured: {} <==", threads);

    TimerClass total_time;
    spdlog::info("==> Calculating INPUT_SIZE <==");
    const uint64_t stream_size = estimate_stream_size();
    spdlog::info("==> INPUT SIZE: {} bytes (~{} GiB) <==", stream_size, stream_size / (1024.0 * 1024.0 * 1024.0));

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

    spdlog::info("->[Timer] : Total OMP Sorting Time -> {}", total_time.result());
    spdlog::info("==> Completed: Merge Sort, output -> {} <==", DATA_OUTPUT);
    return EXIT_SUCCESS;
}