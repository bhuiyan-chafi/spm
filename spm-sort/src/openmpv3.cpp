/**
 * @file openmpv3.cpp
 * @file ./discussions/openmpv3.md
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

namespace helpers
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
        // ranges.size() = parts
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
        uint64_t accumulated_total = 0ULL; // Track total bytes read across all segments

        explicit SegmentReader(const std::string &path) : in(path, std::ios::binary)
        {
            if (!in)
            {
                // spdlog::error("SegmentReader: cannot open input {}", path);
                throw std::runtime_error("Input stream error");
            }
        }

        std::unique_ptr<Items> read_next_segment()
        {
            if (eof)
                return nullptr;

            auto segment = std::make_unique<Items>();
            uint64_t accumulated_chunk = 0ULL; // For this chunk only

            if (carry)
            {
                accumulated_chunk += carry->bytes;
                segment->push_back(std::move(carry->item));
                carry.reset();
            }

            while (true)
            {
                uint64_t key;
                CompactPayload payload;
                if (!read_record(in, key, payload))
                {
                    eof = true;
                    break;
                }
                Item next{key, std::move(payload)};
                // checking if this Item can be added in current Distribution
                const uint64_t next_size = record_size_bytes(next);
                if (!segment->empty() && accumulated_chunk + next_size > DISTRIBUTION_CAP)
                {
                    // we can't add this Item because it will cross the DISTRIBUTION_CAP
                    // we will add this in the next segment
                    carry = PendingRecord{Item{next.key, std::move(next.payload)}, next_size};
                    break;
                }
                accumulated_chunk += next_size;
                // adding the Item
                segment->push_back(std::move(next));
            }
            // Items = DISTRIBUTION_CAP read complete, now send this segment
            if (segment->empty())
                return nullptr;
            accumulated_total += accumulated_chunk;
            // sending one segment
            return segment;
        }
    };

    std::vector<Task> process_segment(std::unique_ptr<Items> segment, size_t segment_id,
                                      size_t threads)
    {
        if (!segment || segment->empty())
            return {};
        /**
         * @example:
         * segment->size() = 10'000 was read in one segment
         * ranges = 10'000/4 = 2500 in each range total
         */
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
            tasks.push_back(Task{std::move(slice), false, segment_id, i});
        }

        // segment destroyed here, memory freed because tasks have the items now
        /**
         * @brief if we do a simple math:
         * threads = ranges = parts then why schedule(dynamic)
         * reason is, yes all the workers are getting equal size of data but the question is are
         * they doing same amount of job?
         * No! because one range can have less unsorted data and one can have a slice that is
         * completely sorted in reverse, that thread will work for more time
         * Will they synchronize?
         * - if you force them manually, they will wait for each other
         * - that's why we used dynamic distribution
         */
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
        }

        // Return tasks for collector to process
        return tasks;
    }

    std::string flush_accumulated_tasks(std::vector<std::vector<Task>> &all_tasks)
    {
        static std::atomic<uint64_t> run_id{0};
        const auto id = run_id.fetch_add(1, std::memory_order_relaxed);
        const std::string path = DATA_TMP_DIR + "run_" + std::to_string(id) + ".bin";

        std::error_code ec;
        std::filesystem::create_directories(DATA_TMP_DIR, ec);
        if (ec)
        {
            // spdlog::warn("Could not ensure tmp dir {} exists: {}", DATA_TMP_DIR, ec.message());
        }

        std::ofstream out(path, std::ios::binary);
        if (!out)
            throw std::runtime_error("Cannot open temp file for flush");

        struct HeapItem
        {
            size_t batch_idx;
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
        for (size_t b = 0; b < all_tasks.size(); ++b)
            for (size_t t = 0; t < all_tasks[b].size(); ++t)
                if (all_tasks[b][t].items && !all_tasks[b][t].items->empty())
                    heap.push(HeapItem{b, t, 0, (*all_tasks[b][t].items)[0].key});

        size_t written = 0;
        while (!heap.empty())
        {
            auto current = heap.top();
            heap.pop();
            const Item &item = (*all_tasks[current.batch_idx][current.task_idx].items)[current.item_idx];
            write_record(out, item.key, item.payload);
            ++written;

            const size_t next_idx = current.item_idx + 1;
            if (next_idx < all_tasks[current.batch_idx][current.task_idx].items->size())
                heap.push(HeapItem{current.batch_idx, current.task_idx, next_idx,
                                   (*all_tasks[current.batch_idx][current.task_idx].items)[next_idx].key});
        }

        // spdlog::info("Flushed {} accumulated tasks ({} records) -> {}",
        //              all_tasks.size() * (all_tasks.empty() ? 0 : all_tasks[0].size()),
        //              written, path);
        return path;
    }

    void final_merge(const std::vector<std::string> &run_paths)
    {
        if (run_paths.empty())
        {
            // spdlog::warn("final_merge: no runs to merge");
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
        // spdlog::info("final_merge: wrote {} records -> {}", written, DATA_OUTPUT);

        for (const auto &path : run_paths)
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }
    /**
     * @fn start_processing: the main heat from where everything is orchestrated. Take the number
     * of 'threads' and starts the process.
     * @param reader: the struct item which calls the constructor and creates a input reader buffer
     * @param accumulated_tasks: we are reading input based on = DISTRIBUTION_CAP
     * input_size/memory_cap = result/workers = DISTRIBUTION_CAP, data
     * at one go. Those data are structured in a task(starting point, ending point), and processed
     * by each worker.
     * How workers are getting a task?
     * - after reading one DISTRIBUTION cap, we are sending it for processing
     * - at that time emitter is idle
     * - workers finish their work and send the results as tasks to the collector
     * - control goes back to the emitter and workers become idle
     * - bad pipeline, this is synchronous but not parallel to the level we wanted
     * To biggest drawback
     * - but it solves the issue of over reading by the emitter which caused memory sky rocketing
     */
    void start_processing(size_t threads)
    {
        TimerClass distribution_time;
        TimerClass sort_time;
        TimerClass merging_time;

        SegmentReader reader(DATA_INPUT);
        // how many segments we are reading
        std::vector<std::string> all_run_paths;
        // generating tasks
        std::vector<std::vector<Task>> accumulated_tasks;
        size_t segment_id = 0;
        // starts the distribution and sorting
        while (true)
        {
            // starts with first segment in first loop
            std::unique_ptr<Items> segment;
            {
                TimerScope ts(distribution_time);
                segment = reader.read_next_segment();
                // breaks when we have an empty segment -> EOF
                if (!segment)
                    break;
            }

            bool is_final = reader.eof;

            // spdlog::info("Processing segment {} ({} items, accumulated_total={} bytes, final={})",
            //              segment_id, segment->size(), reader.accumulated_total,
            //              is_final ? "true" : "false");
            // I have one 'segment'
            std::vector<Task> tasks;
            {
                TimerScope ts(sort_time);
                tasks = process_segment(std::move(segment), segment_id,
                                        threads ? threads : 1);
            }

            // Accumulate sorted tasks
            // spdlog::info("[MEM-A] Before accumulating batch {}: accumulated_tasks.size()={}",
            //              segment_id, accumulated_tasks.size());
            accumulated_tasks.push_back(std::move(tasks));
            // spdlog::info("[MEM-B] After accumulating batch {}: accumulated_tasks.size()={}, capacity={}",
            //              segment_id, accumulated_tasks.size(), accumulated_tasks.capacity());

            // Check if we need to flush
            bool should_flush = (reader.accumulated_total >= MEMORY_CAP) || is_final;

            if (should_flush && !accumulated_tasks.empty())
            {
                TimerScope ts(merging_time);

                if (all_run_paths.empty() && is_final && reader.accumulated_total < MEMORY_CAP)
                {
                    // Special case: Never exceeded MEMORY_CAP, write directly to output
                    report.METHOD = "IN_MEMORY";
                    // spdlog::info("[MEM-C] In-memory path: writing {} batches directly to output", accumulated_tasks.size());
                    // spdlog::info("[MEM-C] accumulated_tasks capacity: {}, total batches: {}",
                    //              accumulated_tasks.capacity(), accumulated_tasks.size());

                    // Count total memory in tasks
                    // size_t total_items = 0;
                    // size_t total_capacity = 0;
                    // for (size_t b = 0; b < accumulated_tasks.size(); ++b)
                    // {
                    //     for (size_t t = 0; t < accumulated_tasks[b].size(); ++t)
                    //     {
                    //         if (accumulated_tasks[b][t].items)
                    //         {
                    //             total_items += accumulated_tasks[b][t].items->size();
                    //             total_capacity += accumulated_tasks[b][t].items->capacity();
                    //         }
                    //     }
                    // }
                    // spdlog::info("[MEM-C] Total items: {}, total vector capacity: {}, overhead: {}%",
                    //              total_items, total_capacity,
                    //              (total_capacity > total_items) ? ((total_capacity - total_items) * 100 / total_items) : 0);

                    // spdlog::info("[MEM-D] Opening output file and building heap...");
                    std::ofstream out(DATA_OUTPUT, std::ios::binary);
                    if (!out)
                        throw std::runtime_error("Cannot open output file");

                    struct HeapItem
                    {
                        size_t batch_idx;
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
                    // spdlog::info("[MEM-E] Building heap from {} batches...", accumulated_tasks.size());
                    for (size_t b = 0; b < accumulated_tasks.size(); ++b)
                        for (size_t t = 0; t < accumulated_tasks[b].size(); ++t)
                            if (accumulated_tasks[b][t].items && !accumulated_tasks[b][t].items->empty())
                                heap.push(HeapItem{b, t, 0, (*accumulated_tasks[b][t].items)[0].key});

                    // spdlog::info("[MEM-F] Heap built with {} entries, starting K-way merge write...", heap.size());
                    size_t written = 0;
                    // size_t progress_interval = 10000000; // Log every 10M records
                    while (!heap.empty())
                    {
                        auto current = heap.top();
                        heap.pop();
                        const Item &item = (*accumulated_tasks[current.batch_idx][current.task_idx].items)[current.item_idx];
                        write_record(out, item.key, item.payload);
                        ++written;

                        // if (written % progress_interval == 0)
                        // {
                        //     spdlog::info("[MEM-PROGRESS] Written {} records", written);
                        // }

                        const size_t next_idx = current.item_idx + 1;
                        if (next_idx < accumulated_tasks[current.batch_idx][current.task_idx].items->size())
                            heap.push(HeapItem{current.batch_idx, current.task_idx, next_idx,
                                               (*accumulated_tasks[current.batch_idx][current.task_idx].items)[next_idx].key});
                    }
                    // spdlog::info("[MEM-G] K-way merge complete, wrote {} records", written);
                    // spdlog::info("[MEM-H] Flushing output stream...");
                    out.flush();
                    // spdlog::info("[MEM-I] Closing output file...");
                    out.close();
                    // spdlog::info("[MEM-J] Clearing accumulated_tasks...");
                    // spdlog::info("Collector(inmem): wrote {} records -> {}", written, DATA_OUTPUT);
                    accumulated_tasks.clear();
                    // spdlog::info("[MEM-K] accumulated_tasks cleared, memory should be freed now");
                }
                else
                {
                    // Flush accumulated tasks to temp file
                    report.METHOD = "MEMORY_OOC";
                    std::string run_path = flush_accumulated_tasks(accumulated_tasks);
                    all_run_paths.push_back(run_path);
                    accumulated_tasks.clear();
                    reader.accumulated_total = 0; // Reset counter after flush
                }
            }

            ++segment_id;
        }

        // spdlog::info("Processed {} segments, produced {} run files", segment_id, all_run_paths.size());

        // Final merge phase (if we created temp files)
        if (!all_run_paths.empty())
        {
            TimerScope ts(merging_time);
            final_merge(all_run_paths);
        }

        report.WORKING_TIME = sort_time.result();
        // spdlog::info("[Timer] Emitter: {}", distribution_time.result());
        // spdlog::info("[Timer] Worker : {}", sort_time.result());
        // spdlog::info("[Timer] Collector: {}", merging_time.result());
    }
}

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

    try
    {
        TimerScope total_scope(total_time);
        helpers::start_processing(threads);
    }
    catch (const std::exception &error)
    {
        spdlog::error("==> X Operation aborted due to: {} X <==", error.what());
        return EXIT_FAILURE;
    }

    // spdlog::info("->[Timer] : Total OMP Sorting Time -> {}", total_time.result());
    // spdlog::info("==> Completed: Merge Sort, output -> {} <==", DATA_OUTPUT);
    report.TOTAL_TIME = total_time.result();
    spdlog::info("M: {} | R: {} | PS: {} | W: {} | DC:{}MiB | WT: {} | TT: {}", report.METHOD, report.RECORDS, report.PAYLOAD_SIZE, report.WORKERS, DISTRIBUTION_CAP / IN_MB, report.WORKING_TIME, report.TOTAL_TIME);
    return EXIT_SUCCESS;
}