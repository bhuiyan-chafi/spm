#include "main.hpp"

// Global timing storage for pipeline stages
static std::string g_emitter_time = "0";
static std::string g_collector_time = "0";

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
        long long sort_time_ns = 0; // Pure worker sort time for this slice
    };

    // WriteTask represents a complete segment ready to write
    struct WriteTask
    {
        std::vector<std::vector<SortedTask>> batches;
        size_t segment_id = 0;
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

        // spdlog::info("[Merge] Wrote {} records -> {}", written, output_path);

        // Aggressively free memory after writing
        for (auto &batch : batches)
        {
            for (auto &task : batch)
            {
                if (task.items)
                {
                    task.items->clear();
                    task.items->shrink_to_fit();
                }
            }
            batch.clear();
            batch.shrink_to_fit();
        }
        batches.clear();
        batches.shrink_to_fit();
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

        // spdlog::info("[Final Merge] Wrote {} records -> {}", written, DATA_OUTPUT);

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
            TimerClass segment_timer;
            segment_timer.start();

            auto segment = reader.read_next_segment();
            if (!segment)
                break;

            segment_timer.stop();
            // spdlog::info("[Emitter] Segment {} disk read time: {}", segment_id, segment_timer.result());

            // makes each slice = size of l1_data_cache of executing machine
            auto ranges = slice_ranges(segment->size(), DEGREE);

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

        g_emitter_time = timer.result();
        // spdlog::info("[Timer] Emitter: {}", g_emitter_time);
    }

    void worker_stage(SafeQueue<Task> &task_queue, SafeQueue<SortedTask> &sorted_queue,
                      std::atomic<size_t> &tasks_sorted, int worker_id,
                      std::vector<std::atomic<uint64_t>> &worker_times_ns)
    {
        // unsigned long tid = get_tid();
        // spdlog::info("[Worker-{} TID:{}] Started", worker_id, tid);
        size_t local_count = 0;
        TimerClass local_timer;

        while (true)
        {
            Task task;
            if (!task_queue.pop(task))
                break;

            if (task.is_poison)
            {
                sorted_queue.push(SortedTask{nullptr, 0, 0, true});
                // spdlog::info("[Worker-{} TID:{}] Poison received, sorted {} tasks TOTAL in {}",
                //              worker_id, tid, local_count, local_timer.result());
                break;
            }

            // spdlog::info("[Worker-{} TID:{}] Processing segment_{} slice_{} ({} items) [Task #{}]",
            //              worker_id, tid, task.segment_id, task.slice_index,
            //              task.items->size(), local_count + 1);

            TimerClass slice_timer;
            slice_timer.start();
            std::sort(task.items->begin(), task.items->end(),
                      [](const Item &a, const Item &b)
                      { return a.key < b.key; });
            slice_timer.stop();

            local_timer.add_elapsed(slice_timer.elapsed_ns());

            // spdlog::info("[Worker-{} TID:{}] Completed segment_{} slice_{} in {} [Task #{}]",
            //              worker_id, tid, task.segment_id, task.slice_index,
            //              slice_timer.result(), local_count + 1);

            SortedTask result{std::move(task.items), task.segment_id, task.slice_index, false, slice_timer.elapsed_ns().count()};
            sorted_queue.push(std::move(result));
            local_count++;
            tasks_sorted.fetch_add(1, std::memory_order_relaxed);
        }

        worker_times_ns[worker_id].store(local_timer.elapsed_ns().count(), std::memory_order_relaxed);
        // spdlog::info("[Worker-{}] Completed: {} tasks sorted, {} total work time",
        //              tid, local_count, local_timer.result());
    }

    // ==================== WRITER THREAD POOL ====================

    void writer_stage(SafeQueue<WriteTask> &write_queue,
                      std::vector<std::string> &run_paths,
                      std::mutex &paths_mutex,
                      std::atomic<uint64_t> &total_write_time_ns)
    {
        // unsigned long tid = get_tid();
        // spdlog::info("[Writer-{}] Started", tid);
        size_t segments_written = 0;
        TimerClass local_timer;

        while (true)
        {
            WriteTask write_task;
            if (!write_queue.pop(write_task))
                break;

            if (write_task.is_poison)
            {
                // spdlog::info("[Writer-{} TID:{}] Poison received, wrote {} segments in {}",
                //              writer_id, tid, segments_written,
                //              local_timer.result());
                break;
            }

            // spdlog::info("[Writer-{} TID:{}] Writing segment {} to disk",
            //              writer_id, tid, write_task.segment_id);

            local_timer.start();
            std::string run_path = flush_to_disk(write_task.batches);
            local_timer.stop();

            // spdlog::info("[Writer-{} TID:{}] Segment {} written to disk in {} -> {}",
            //              writer_id, tid, write_task.segment_id,
            //              local_timer.result(), run_path);

            // Explicitly clear batches to free memory immediately
            write_task.batches.clear();
            write_task.batches.shrink_to_fit();

            // Force memory return to OS
            malloc_trim(0);

            // spdlog::info("[Writer-{} TID:{}] Segment {} complete: write={}, cleanup={}, total={} - Memory freed",
            //              writer_id, tid, write_task.segment_id,
            //              TimerClass::humanize_ns(write_duration_ns),
            //              TimerClass::humanize_ns(cleanup_duration_ns),
            //              TimerClass::humanize_ns(total_duration_ns));
            {
                std::lock_guard<std::mutex> lock(paths_mutex);
                run_paths.push_back(run_path);
            }

            segments_written++;
        }

        total_write_time_ns.fetch_add(local_timer.elapsed_ns().count(), std::memory_order_relaxed);
        // spdlog::info("[Writer-{} TID:{}] Finished: {} segments written, total time {}",
        //              writer_id, tid, segments_written,
        //              local_timer.result());
    } // ==================== collector (formerly Collector) ====================

    void collector_stage(SafeQueue<SortedTask> &sorted_queue,
                         SafeQueue<WriteTask> &write_queue,
                         size_t num_workers,
                         size_t num_writers,
                         bool write_segments)
    {
        TimerClass timer;
        TimerScope ts(timer);

        // Track segments: segment_id -> {tasks, expected_count, bytes}
        struct SegmentInfo
        {
            std::vector<SortedTask> tasks;
            size_t expected_slices;
            uint64_t total_bytes = 0;
            long long total_cpu_time_ns = 0;
            bool timing_started = false;
        };

        std::map<size_t, SegmentInfo> segments;
        size_t poison_count = 0;
        const size_t slices_per_segment = DEGREE; // as many slices we did to one segment=DISTRIBUTION_CAP
        uint64_t accumulated_bytes = 0;

        // if (write_segments)
        // {
        //     spdlog::info("[collector] Mode: OUT-OF-CORE - Writing segments individually via writer pool");
        // }
        // else
        // {
        //     spdlog::info("[collector] Mode: IN-MEMORY - Accumulating all segments for single write");
        // }

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

            // Calculate task size
            uint64_t task_bytes = 0;
            for (const auto &item : *sorted.items)
                task_bytes += record_size_bytes(item);

            // Capture segment_id before move
            size_t current_segment_id = sorted.segment_id;

            // Add to segment
            auto &seg_info = segments[current_segment_id];

            // Start timing on first slice
            if (!seg_info.timing_started)
            {
                seg_info.timing_started = true;
            }

            seg_info.total_cpu_time_ns += sorted.sort_time_ns;
            seg_info.tasks.push_back(std::move(sorted));
            seg_info.total_bytes += task_bytes;
            seg_info.expected_slices = slices_per_segment;

            // Check if segment is complete
            if (seg_info.tasks.size() == seg_info.expected_slices)
            {
                // spdlog::info("[Workers] Segment {} accumulated CPU time: {}",
                //              current_segment_id,
                //              TimerClass::humanize_ns(seg_info.total_cpu_time_ns));

                // double mb_size = static_cast<double>(seg_info.total_bytes) / (1024.0 * 1024.0);
                // size_t task_count = seg_info.tasks.size();

                if (write_segments)
                {
                    // OUT-OF-CORE MODE: Write segment immediately via writer pool
                    // spdlog::info("[collector] Segment {} complete ({} slices, {:.2f} MB) - Sending to writer pool",
                    //              current_segment_id, task_count, mb_size);

                    // Prepare write task
                    std::vector<std::vector<SortedTask>> single_segment_batch;
                    single_segment_batch.push_back(std::move(seg_info.tasks));

                    // Send to writer pool (may block if writers are busy)
                    write_queue.push(WriteTask{std::move(single_segment_batch), current_segment_id, false});

                    // Free memory immediately from collector's map
                    segments.erase(current_segment_id);
                    // spdlog::info("[collector] Segment {} removed from collector memory", current_segment_id);
                }
                else
                {
                    // IN-MEMORY MODE: Keep accumulating
                    // spdlog::info("[collector] Segment {} complete ({} slices, {:.2f} MB) - Accumulating in memory",
                    //              current_segment_id, task_count, mb_size);
                    accumulated_bytes += seg_info.total_bytes;
                    // Keep segment in memory, don't erase
                }
            }
        }

        // Handle end-of-pipeline
        if (write_segments)
        {
            // OUT-OF-CORE: Handle any remaining incomplete segments
            if (!segments.empty())
            {
                // size_t incomplete_count = segments.size();
                // spdlog::info("[collector] Processing {} incomplete segments", incomplete_count);
                for (auto &[seg_id, seg_info] : segments)
                {
                    if (!seg_info.tasks.empty())
                    {
                        // size_t task_count = seg_info.tasks.size();
                        // spdlog::info("[collector] Flushing incomplete segment {} ({} slices)",
                        //              seg_id, task_count);
                        std::vector<std::vector<SortedTask>> single_segment_batch;
                        single_segment_batch.push_back(std::move(seg_info.tasks));
                        write_queue.push(WriteTask{std::move(single_segment_batch), seg_id, false});
                    }
                }
            }

            // Send poison pills to writers
            // spdlog::info("[collector] Sending poison pills to {} writers", num_writers);
            for (size_t i = 0; i < num_writers; ++i)
            {
                write_queue.push(WriteTask{{}, 0, true});
            }
        }
        else
        {
            // IN-MEMORY: Write all accumulated segments at once
            if (!segments.empty())
            {
                // double gb_size = static_cast<double>(accumulated_bytes) / (1024.0 * 1024.0 * 1024.0);
                // spdlog::info("[collector] All data accumulated ({:.2f} GB) - Writing directly to output",
                //              gb_size);

                // Flatten all segments into batches structure
                std::vector<std::vector<SortedTask>> all_batches;
                for (auto &[seg_id, seg_info] : segments)
                {
                    all_batches.push_back(std::move(seg_info.tasks));
                }

                // Write directly to output (no intermediate runs)
                merge_batches_to_file(all_batches, DATA_OUTPUT);
                // spdlog::info("[collector] Direct write to output completed");
            }

            // No need for writers, send poison pills anyway to clean up
            for (size_t i = 0; i < num_writers; ++i)
            {
                write_queue.push(WriteTask{{}, 0, true});
            }
        }

        g_collector_time = timer.result();
        // spdlog::info("[Timer] collector: {}", g_collector_time);
        // spdlog::info("[collector] Completed");
    }

    // ==================== MAIN PIPELINE ====================

    long long run_farm(size_t num_workers, size_t num_writers = WORKERS / 2)
    {
        // Determine if we need out-of-core processing
        bool write_segments = (INPUT_BYTES > MEMORY_CAP);
        if (write_segments)
            report.METHOD = "MEMORY_OOC";
        SafeQueue<Task> task_queue(DEGREE * WORKERS);
        SafeQueue<SortedTask> sorted_queue(DEGREE * WORKERS);

        // Write queue: Keep it minimal to avoid buffering segments
        // num_writers (2) means max 2 segments buffered in write queue
        SafeQueue<WriteTask> write_queue(num_writers); // One per writer - no extra buffering

        std::atomic<size_t> tasks_sorted{0};
        std::vector<std::atomic<uint64_t>> worker_times_ns(num_workers);
        for (size_t i = 0; i < num_workers; ++i)
            worker_times_ns[i].store(0, std::memory_order_relaxed);
        std::atomic<uint64_t> total_write_time_ns{0};

        // Shared state for writer threads
        std::vector<std::string> run_paths;
        std::mutex paths_mutex;

        // spdlog::info("==> Starting OMP Farm: {} workers, {} writers <==", num_workers, num_writers);
        // spdlog::info("==> Task Queue Max Size: {} tasks <==", num_workers * 4);
        // spdlog::info("==> DISTRIBUTION_CAP: {} MB <==", DISTRIBUTION_CAP / IN_MB);
        // spdlog::info("==> MEMORY_CAP: {} GB <==", MEMORY_CAP / IN_GB);

        // Total threads: 1 emitter + num_workers + 1 collector + num_writers
        // Here we are generating 2+num_writers extra threads
        size_t total_threads = 1 + num_workers + 1 + num_writers;

// OpenMP parallel region with tasks
#pragma omp parallel num_threads(total_threads)
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
                        worker_stage(task_queue, sorted_queue, tasks_sorted, i, worker_times_ns);
                    }
                }

// collector task (organizes segments)
#pragma omp task
                {
                    collector_stage(sorted_queue, write_queue, num_workers, num_writers, write_segments);
                }

                // Writer tasks (parallel writers)
                for (size_t i = 0; i < num_writers; ++i)
                {
#pragma omp task
                    {
                        writer_stage(write_queue, run_paths, paths_mutex, total_write_time_ns);
                    }
                }
            }
        }

        // spdlog::info("[Pipeline] All workers and writers finished");
        // spdlog::info("==> Total tasks sorted: {} <==", tasks_sorted.load());

        // Calculate accumulated and overlapped times
        long long total_worker_time_ns = 0;
        long long max_worker_time_ns = 0;
        for (size_t i = 0; i < num_workers; ++i)
        {
            long long wt = worker_times_ns[i].load(std::memory_order_relaxed);
            total_worker_time_ns += wt;
            if (wt > max_worker_time_ns)
                max_worker_time_ns = wt;
        }

        // spdlog::info("==> Total worker time (accumulated): {} <==",
        //              TimerClass::humanize_ns(total_worker_time_ns));
        report.WORKING_TIME = TimerClass::humanize_ns(total_worker_time_ns);
        if (write_segments && total_write_time_ns.load() > 0)
        {
            // spdlog::info("==> Total intermediate write time (accumulated): {} <==",
            //              TimerClass::humanize_ns(total_write_time_ns.load()));
            // spdlog::info("==> Number of intermediate runs written: {} <==", run_paths.size());
        }

        // Final merge of all segment runs (only if write_segments was true)
        if (write_segments)
        {
            if (run_paths.empty())
            {
                // spdlog::warn("[Final] No runs to merge - output should have been written directly");
            }
            else if (run_paths.size() == 1)
            {
                // spdlog::info("[Final] Single run file - renaming to output");
                std::filesystem::rename(run_paths[0], DATA_OUTPUT);
            }
            else
            {
                // spdlog::info("[Final] Performing final k-way merge of {} runs", run_paths.size());
                final_merge(run_paths);
            }
        }
        else
        {
            // spdlog::info("[Final] In-memory mode - output already written directly by collector");
        }

        return max_worker_time_ns;
    }

} // namespace farm

// ==================== MAIN ====================

int main(int argc, char **argv)
{
    parse_cli_and_set(argc, argv);
    size_t threads = (WORKERS > 0) ? static_cast<size_t>(WORKERS) : 1;

    // spdlog::info("==> OMP Farm Configuration <==");
    // spdlog::info("==> Input: {} <==", DATA_INPUT);
    // spdlog::info("==> Workers: {} <==", threads);

    TimerClass total_time;
    long long max_worker_time_ns = 0;

    try
    {
        TimerScope total_scope(total_time);
        max_worker_time_ns = farm::run_farm(threads);
    }
    catch (const std::exception &error)
    {
        spdlog::error("==> X Operation failed: {} X <==", error.what());
        return EXIT_FAILURE;
    }

    // spdlog::info("->[Timer] Total execution time: {}", total_time.result());
    // spdlog::info("==> Completed successfully -> {} <==", DATA_OUTPUT);
    report.TOTAL_TIME = total_time.result();

    // Log overlapped (parallel) time
    if (max_worker_time_ns > 0)
    {
        // spdlog::info("Workers overlapped (parallel) time: {}", TimerClass::humanize_ns(max_worker_time_ns));
    }

    spdlog::info("M: {} | R: {} | PS: {} | W: {} | DC:{}MiB | ET: {} | WT: {} | CT: {} | TT: {}", report.METHOD, report.RECORDS, report.PAYLOAD_SIZE, report.WORKERS, DISTRIBUTION_CAP / IN_MB, g_emitter_time, report.WORKING_TIME, g_collector_time, report.TOTAL_TIME);
    return EXIT_SUCCESS;
}
