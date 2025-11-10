/**
 * @brief OpenMP Farm Pattern Implementation
 * @description Implements concurrent emit → work → collect pipeline using OpenMP threads
 *
 * Architecture:
 * - Emitter Thread: Reads segments and creates tasks
 * - Worker Threads: Sort tasks in parallel
 * - Collector Thread: Merges sorted results
 *
 * Synchronization:
 * - Thread-safe queues with mutexes and condition variables
 * - Lock-based coordination between stages
 * - Memory cap enforcement with out-of-core support
 */
#include "main.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <omp.h>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace farm
{
    using Items = std::vector<Item>;

    // ==================== DATA STRUCTURES ====================

    struct PendingRecord
    {
        Item item;
        uint64_t bytes;
    };

    // Task represents work for workers
    struct Task
    {
        std::unique_ptr<Items> items;
        size_t segment_id = 0;
        size_t slice_index = 0;
        bool is_poison = false;
    };

    // SortedTask represents completed work
    struct SortedTask
    {
        std::unique_ptr<Items> items;
        size_t segment_id = 0;
        size_t slice_index = 0;
        bool is_poison = false;
    };

    // ==================== THREAD-SAFE QUEUE ====================

    template <typename T>
    class SafeQueue
    {
        std::queue<T> queue_;
        std::mutex mutex_;
        std::condition_variable cv_consumer_; // For pop() operations
        std::condition_variable cv_producer_; // For push() operations
        bool closed_ = false;
        size_t max_size_;

    public:
        explicit SafeQueue(size_t max_size = 0) : max_size_(max_size) {}

        void push(T item)
        {
            std::unique_lock<std::mutex> lock(mutex_);

            // Backpressure: wait if queue is full
            if (max_size_ > 0)
            {
                cv_producer_.wait(lock, [this]
                                  { return queue_.size() < max_size_ || closed_; });
            }

            queue_.push(std::move(item));
            cv_consumer_.notify_one(); // Wake up a waiting consumer
        }

        bool pop(T &item)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_consumer_.wait(lock, [this]
                              { return !queue_.empty() || closed_; });
            if (queue_.empty())
                return false;
            item = std::move(queue_.front());
            queue_.pop();
            cv_producer_.notify_one(); // Wake up a waiting producer
            return true;
        }
    };

    // ==================== HELPER FUNCTIONS ====================

    inline uint64_t record_size_bytes(const Item &item)
    {
        return sizeof(uint64_t) + sizeof(uint32_t) + item.payload.size();
    }

    std::vector<std::pair<size_t, size_t>> slice_ranges(size_t n, size_t parts)
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

    // ==================== SEGMENT READER ====================

    struct SegmentReader
    {
        std::ifstream in;
        std::optional<PendingRecord> carry;
        bool eof = false;
        uint64_t accumulated_total = 0ULL;

        explicit SegmentReader(const std::string &path) : in(path, std::ios::binary)
        {
            if (!in)
                throw std::runtime_error("cannot open input: " + path);
        }

        std::unique_ptr<Items> read_next_segment()
        {
            if (eof)
                return nullptr;

            auto segment = std::make_unique<Items>();
            uint64_t accumulated_chunk = 0ULL;

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
                const uint64_t next_size = record_size_bytes(next);

                if (!segment->empty() && accumulated_chunk + next_size > DISTRIBUTION_CAP)
                {
                    carry = PendingRecord{Item{next.key, std::move(next.payload)}, next_size};
                    break;
                }

                accumulated_chunk += next_size;
                segment->push_back(std::move(next));
            }

            if (segment->empty())
                return nullptr;

            accumulated_total += accumulated_chunk;
            return segment;
        }
    };

    // ==================== MERGE FUNCTIONS ====================

    struct HeapItem
    {
        size_t batch_idx, task_idx, item_idx;
        uint64_t key;
        bool operator>(const HeapItem &other) const { return key > other.key; }
    };

    void merge_batches_to_file(std::vector<std::vector<SortedTask>> &batches,
                               const std::string &output_path)
    {
        std::ofstream out(output_path, std::ios::binary);
        if (!out)
            throw std::runtime_error("Cannot open: " + output_path);

        std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;

        for (size_t b = 0; b < batches.size(); ++b)
            for (size_t t = 0; t < batches[b].size(); ++t)
                if (batches[b][t].items && !batches[b][t].items->empty())
                    heap.push(HeapItem{b, t, 0, (*batches[b][t].items)[0].key});

        size_t written = 0;
        while (!heap.empty())
        {
            auto current = heap.top();
            heap.pop();
            const Item &item = (*batches[current.batch_idx][current.task_idx].items)[current.item_idx];
            write_record(out, item.key, item.payload);
            ++written;

            const size_t next_idx = current.item_idx + 1;
            if (next_idx < batches[current.batch_idx][current.task_idx].items->size())
                heap.push(HeapItem{current.batch_idx, current.task_idx, next_idx,
                                   (*batches[current.batch_idx][current.task_idx].items)[next_idx].key});
        }

        spdlog::info("[Merge] Wrote {} records -> {}", written, output_path);
    }

    std::string flush_to_disk(std::vector<std::vector<SortedTask>> &batches)
    {
        static std::atomic<uint64_t> run_id{0};
        const std::string path = DATA_TMP_DIR + "run_" + std::to_string(run_id.fetch_add(1)) + ".bin";
        std::filesystem::create_directories(DATA_TMP_DIR);
        merge_batches_to_file(batches, path);
        return path;
    }

    void final_merge(const std::vector<std::string> &run_paths)
    {
        if (run_paths.empty())
        {
            std::ofstream(DATA_OUTPUT, std::ios::binary);
            return;
        }

        std::ofstream out(DATA_OUTPUT, std::ios::binary);
        if (!out)
            throw std::runtime_error("cannot open output");

        std::vector<std::unique_ptr<TempReader>> readers;
        readers.reserve(run_paths.size());
        for (const auto &path : run_paths)
            readers.push_back(std::make_unique<TempReader>(path));

        struct HeapNode
        {
            uint64_t key;
            size_t run_index;
            bool operator>(const HeapNode &other) const { return key > other.key; }
        };

        std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> heap;
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

        spdlog::info("[Final Merge] Wrote {} records -> {}", written, DATA_OUTPUT);

        for (const auto &path : run_paths)
            std::filesystem::remove(path);
    }

    // ==================== PIPELINE STAGES ====================

    void emitter_stage(SafeQueue<Task> &task_queue, size_t num_workers)
    {
        TimerClass timer;
        TimerScope ts(timer);
        SegmentReader reader(DATA_INPUT);
        size_t segment_id = 0;

        while (true)
        {
            auto segment = reader.read_next_segment();
            if (!segment)
                break;

            auto ranges = slice_ranges(segment->size(), num_workers * 2);

            for (size_t i = 0; i < ranges.size(); ++i)
            {
                auto [L, R] = ranges[i];
                auto slice = std::make_unique<Items>();
                slice->reserve(R - L);
                for (size_t j = L; j < R; ++j)
                    slice->push_back(std::move((*segment)[j]));

                task_queue.push(Task{std::move(slice), segment_id, i, false});
            }
            segment_id++;
        }

        // Send poison pills
        for (size_t i = 0; i < num_workers; ++i)
            task_queue.push(Task{nullptr, 0, 0, true});
    }

    void worker_stage(SafeQueue<Task> &task_queue, SafeQueue<SortedTask> &sorted_queue,
                      std::atomic<size_t> &tasks_sorted, int worker_id,
                      std::atomic<uint64_t> &total_worker_time_ms)
    {
        unsigned long tid = get_tid();
        spdlog::info("[Worker-{} TID:{}] Started", worker_id, tid);
        size_t local_count = 0;
        uint64_t local_time_ms = 0;

        while (true)
        {
            Task task;
            if (!task_queue.pop(task))
                break;

            if (task.is_poison)
            {
                sorted_queue.push(SortedTask{nullptr, 0, 0, true});
                spdlog::info("[Worker-{} TID:{}] Poison received, sorted {} tasks TOTAL in {}ms",
                             worker_id, tid, local_count, local_time_ms);
                break;
            }

            spdlog::info("[Worker-{} TID:{}] Processing segment_{} slice_{} ({} items) [Task #{}]",
                         worker_id, tid, task.segment_id, task.slice_index,
                         task.items->size(), local_count + 1);

            auto start = std::chrono::steady_clock::now();
            std::sort(task.items->begin(), task.items->end(),
                      [](const Item &a, const Item &b)
                      { return a.key < b.key; });
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
            local_time_ms += duration_ms;

            spdlog::info("[Worker-{} TID:{}] Completed segment_{} slice_{} in {}ms [Task #{}]",
                         worker_id, tid, task.segment_id, task.slice_index,
                         duration_ms, local_count + 1);

            sorted_queue.push(SortedTask{std::move(task.items), task.segment_id, task.slice_index, false});
            local_count++;
            tasks_sorted.fetch_add(1, std::memory_order_relaxed);
        }

        total_worker_time_ms.fetch_add(local_time_ms, std::memory_order_relaxed);
        spdlog::info("[Worker-{}] Completed: {} tasks sorted, {}ms total work time",
                     tid, local_count, local_time_ms);
    }

    void collector_stage(SafeQueue<SortedTask> &sorted_queue, size_t num_workers)
    {
        TimerClass timer;
        TimerScope ts(timer);
        std::vector<std::vector<SortedTask>> batches;
        std::vector<std::string> run_paths;
        size_t poison_count = 0;
        uint64_t accumulated_bytes = 0;

        while (poison_count < num_workers)
        {
            SortedTask sorted;
            if (!sorted_queue.pop(sorted))
                break;

            if (sorted.is_poison)
            {
                poison_count++;
                continue;
            }

            uint64_t task_bytes = 0;
            for (const auto &item : *sorted.items)
                task_bytes += record_size_bytes(item);

            if (batches.size() <= sorted.segment_id)
                batches.resize(sorted.segment_id + 1);

            batches[sorted.segment_id].push_back(std::move(sorted));
            accumulated_bytes += task_bytes;

            if (accumulated_bytes >= MEMORY_CAP)
            {
                run_paths.push_back(flush_to_disk(batches));
                batches.clear();
                accumulated_bytes = 0;
            }
        }

        // Final processing
        if (!batches.empty())
        {
            if (run_paths.empty())
                merge_batches_to_file(batches, DATA_OUTPUT);
            else
            {
                run_paths.push_back(flush_to_disk(batches));
                batches.clear();
            }
        }

        if (!run_paths.empty())
            final_merge(run_paths);
    }

    // ==================== MAIN PIPELINE ====================

    void run_farm(size_t num_workers)
    {
        // Bounded queues to enforce memory cap
        // task_queue: limit to 4x workers (allows some buffering but prevents unbounded growth)
        SafeQueue<Task> task_queue(num_workers * 1);
        SafeQueue<SortedTask> sorted_queue(num_workers * 1);

        std::atomic<size_t> tasks_sorted{0};
        std::atomic<uint64_t> total_worker_time_ms{0};

        spdlog::info("==> Starting OMP Farm: {} workers <==", num_workers);
        spdlog::info("==> Task Queue Max Size: {} tasks <==", num_workers * 4);
        spdlog::info("==> DISTRIBUTION_CAP: {} MB <==", DISTRIBUTION_CAP / (1024 * 1024));
        spdlog::info("==> MEMORY_CAP: {} GB <==", MEMORY_CAP / (1024 * 1024 * 1024));

// OpenMP parallel region with tasks
#pragma omp parallel num_threads(num_workers + 2)
        {
#pragma omp single
            {
// Emitter task
#pragma omp task
                {
                    emitter_stage(task_queue, num_workers);
                }

                // Worker tasks
                for (size_t i = 0; i < num_workers; ++i)
                {
#pragma omp task firstprivate(i)
                    {
                        worker_stage(task_queue, sorted_queue, tasks_sorted, i, total_worker_time_ms);
                    }
                }

// Collector task
#pragma omp task
                {
                    collector_stage(sorted_queue, num_workers);
                }
            }
        }

        spdlog::info("[Pipeline] All workers finished");
        spdlog::info("==> Total tasks sorted: {} <==", tasks_sorted.load());
        spdlog::info("==> Total worker time (accumulated): {} ms ({:.2f} s) <==",
                     total_worker_time_ms.load(), total_worker_time_ms.load() / 1000.0);
    }

} // namespace farm

// ==================== MAIN ====================

int main(int argc, char **argv)
{
    parse_cli_and_set(argc, argv);

    size_t threads = (WORKERS > 0) ? static_cast<size_t>(WORKERS) : 1;

    spdlog::info("==> OMP Farm Configuration <==");
    spdlog::info("==> Input: {} <==", DATA_INPUT);
    spdlog::info("==> Workers: {} <==", threads);

    TimerClass total_time;

    try
    {
        TimerScope total_scope(total_time);
        farm::run_farm(threads);
    }
    catch (const std::exception &error)
    {
        spdlog::error("==> X Operation failed: {} X <==", error.what());
        return EXIT_FAILURE;
    }

    spdlog::info("->[Timer] Total execution time: {}", total_time.result());
    spdlog::info("==> Completed successfully -> {} <==", DATA_OUTPUT);
    return EXIT_SUCCESS;
}
