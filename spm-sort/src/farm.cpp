#include "main.hpp"

// Global timing storage for pipeline stages
static std::string g_emitter_time = "0";
static std::string g_collector_time = "0";

/**
 * -------------- Utility Functions --------------
 * @param slice_ranges: hold the ranges of data for a worker
 */

static inline std::vector<std::pair<size_t, size_t>>
slice_ranges(size_t n, size_t parts)
{
    if (parts == 0)
        parts = 1;
    std::vector<std::pair<size_t, size_t>> r;
    r.reserve(parts);
    for (size_t i = 0; i < parts; ++i)
    {
        size_t L = (i * n) / parts, R = ((i + 1) * n) / parts;
        r.emplace_back(L, R);
    }
    return r;
}

/**
 * @struct Task: to define a task to emit to the workers
 * @param spill: is true when our data is bigger than our MEMORY_CAP
 * @param segment_id: if we had to spill, of which segment is the current task
 * @param slice_index: each slice processed by one worker
 *
 * @struct TaskResult: decides type of operation
 * @param InMemBatch: if we have loaded everything in memory
 * @param SortedSlice: sorted slice to be batched by coordinator
 */
struct Task
{
    std::vector<Item> *items;
    bool spill;
    size_t segment_id;
    size_t slice_index;
};

// SortedTask represents a sorted slice from a worker
struct SortedTask
{
    std::vector<Item> *items;
    size_t segment_id;
    size_t slice_index;
};

struct TaskResult
{
    enum class Kind
    {
        InMemBatch,
        SortedSlice // OOC mode: sorted slice for coordinator batching
    } kind;
    std::vector<Item> *items = nullptr;
    bool spill = false;
    size_t segment_id = 0, slice_index = 0;
    long long sort_time_ns = 0; // Pure worker sort time for this slice
};

// ==================== MERGE FUNCTIONS ====================

struct HeapItem
{
    size_t slice_idx, item_idx;
    uint64_t key;
    bool operator>(const HeapItem &other) const { return key > other.key; }
};

inline void merge_slices_to_file(std::vector<SortedTask> &slices, const std::string &output_path)
{
    std::ofstream out(output_path, std::ios::binary);
    if (!out)
        throw std::runtime_error("Cannot open: " + output_path);

    std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;

    // Initialize heap with first item from each slice
    for (size_t s = 0; s < slices.size(); ++s)
        if (slices[s].items && !slices[s].items->empty())
            heap.push(HeapItem{s, 0, (*slices[s].items)[0].key});

    size_t written = 0;
    while (!heap.empty())
    {
        auto current = heap.top();
        heap.pop();
        const Item &item = (*slices[current.slice_idx].items)[current.item_idx];
        write_record(out, item.key, item.payload);
        ++written;

        const size_t next_idx = current.item_idx + 1;
        if (next_idx < slices[current.slice_idx].items->size())
            heap.push(HeapItem{current.slice_idx, next_idx,
                               (*slices[current.slice_idx].items)[next_idx].key});
    }

    // Free memory after writing
    for (auto &slice : slices)
    {
        if (slice.items)
        {
            delete slice.items;
            slice.items = nullptr;
        }
    }
}

inline std::string flush_segment_to_disk(std::vector<SortedTask> &slices)
{
    static std::atomic<uint64_t> run_id{0};
    const std::string path = DATA_TMP_DIR + "run_" + std::to_string(run_id.fetch_add(1)) + ".bin";
    std::filesystem::create_directories(DATA_TMP_DIR);
    merge_slices_to_file(slices, path);
    return path;
}

/**
 * -------------- FARM Stages --------------
 * @param ITEMS: a vector of each record defined in "record.hpp"
 *
 * @struct Emitter: is the distributor that creates a struct TASK and send to one worker
 * @param input_buffer: loads records from input.bin -> MEMORY
 * @param exceeded: indicates that MEMORY_CAP has crossed and we have to write intermediate slices
 *
 * @struct Worker: takes a slice from the Emitter (either of a segment or independent) and sorts it
 * -> if it's a single segment the whole result is sent to Collector and Collector performs the final
 * merge
 * -> if it's a part of segment and more to come then it writes it's own slice and takes the next
 * slice from the emitter to start processing
 *
 * @struct Collector: takes a @struct TaskResult and check if it was in-memory operation or a
 * segmented result
 */
namespace ff_farm
{
    using ITEMS = std::vector<Item>;

    struct Emitter : ff::ff_node_t<Task>
    {
        TimerClass timer_emit;
        explicit Emitter(const std::string &inpath, uint64_t Workers)
            : in(inpath, std::ios::binary), Workers(Workers) {}
        int svc_init() override
        {
            // spdlog::info("[init] Emitter tid={} cpu={}", get_tid(), sched_getcpu());
            if (!in)
                throw std::runtime_error("Emitter: cannot open input");
            return 0;
        }
        Task *svc(Task *) override
        {
            TimerScope ts(timer_emit);
            namespace fs = std::filesystem;
            std::error_code ec;
            const auto stream_bytes = fs::file_size(DATA_INPUT, ec);
            const bool fits_in_memory = (!ec) && stream_bytes <= MEMORY_CAP;

            // if (fits_in_memory)
            // {
            //     spdlog::info("[Emitter] IN-MEMORY path, total bytes={}", stream_bytes);
            // }
            // else
            // {
            //     spdlog::info("[Emitter] OOC path, streaming segments at ~DISTRIBUTION_CAP");
            // }

            size_t segment_id = 0;

            auto emit_segment = [&](std::unique_ptr<ITEMS> seg, TimerClass &segment_timer)
            {
                if (!seg || seg->empty())
                    return;

                auto ranges = slice_ranges(seg->size(), DEGREE);
                // spdlog::info("[Emitter] Emitting segment {} with {} items, {} slices",
                //              segment_id, seg->size(), ranges.size());
                for (size_t i = 0; i < ranges.size(); ++i)
                {
                    auto [L, R] = ranges[i];
                    auto *slice = new ITEMS;
                    slice->reserve(R - L);
                    for (size_t j = L; j < R; ++j)
                        slice->push_back(std::move((*seg)[j]));
                    ff_send_out(new Task{slice, /*spill=*/!fits_in_memory, segment_id, i});
                }
                segment_timer.stop();
                // spdlog::info("[Emitter] Segment {} read+emit: {}", segment_id, segment_timer.result());
                ++segment_id;
            };

            auto segment = std::make_unique<ITEMS>();
            uint64_t accumulator = 0ULL;
            TimerClass segment_timer;
            segment_timer.start();

            while (true)
            {
                uint64_t key;
                CompactPayload payload;
                if (!read_record(in, key, payload))
                {
                    emit_segment(std::move(segment), segment_timer);
                    return EOS;
                }
                const uint64_t record_size = sizeof(uint64_t) + sizeof(uint32_t) + payload.size();
                if (accumulator + record_size > DISTRIBUTION_CAP && !segment->empty())
                {
                    emit_segment(std::move(segment), segment_timer);
                    segment = std::make_unique<ITEMS>();
                    accumulator = 0ULL;
                    segment_timer = TimerClass();
                    segment_timer.start();
                }
                segment->push_back(Item{key, std::move(payload)});
                accumulator += record_size;
            }
        }
        void svc_end() override
        {
            g_emitter_time = timer_emit.result();
            // spdlog::info("[Timer] Emitter: {}", g_emitter_time);
        }

    private:
        std::ifstream in;
        uint64_t Workers;
    };

    struct Worker : ff::ff_node_t<Task, TaskResult>
    {
        Worker(std::shared_ptr<Timings> agg, int idx)
            : agg_(std::move(agg)), idx_(idx) {}
        TimerClass timer_work;
        int svc_init() override
        {
            // spdlog::info("[init] Worker#{} tid={} cpu={}", idx_, get_tid(), sched_getcpu());
            std::filesystem::create_directories(DATA_TMP_DIR);
            return 0;
        }
        TaskResult *svc(Task *task) override
        {
            auto *local_items = task->items;
            // spdlog::info("[Worker-{}] Processing segment_{} slice_{} ({} items)",
            //              idx_, task->segment_id, task->slice_index, local_items->size());

            TimerClass slice_timer;
            slice_timer.start();
            std::sort(local_items->begin(), local_items->end(),
                      [](const Item &a, const Item &b)
                      { return a.key < b.key; });
            slice_timer.stop();

            timer_work.add_elapsed(slice_timer.elapsed_ns());

            // spdlog::info("[Worker-{}] Completed segment_{} slice_{} in {}",
            //              idx_, task->segment_id, task->slice_index, slice_timer.result());

            auto *result = new TaskResult();
            result->spill = task->spill;
            result->segment_id = task->segment_id;
            result->slice_index = task->slice_index;
            result->sort_time_ns = slice_timer.elapsed_ns().count();
            // if it was within memory operation
            if (!task->spill)
            {
                result->kind = TaskResult::Kind::InMemBatch;
                // hand sorted slice to collector
                result->items = local_items;
            }
            else
            {
                // OOC mode: return sorted slice to collector for batching
                result->kind = TaskResult::Kind::SortedSlice;
                result->items = local_items; // Collector will batch DEGREE slices and write once
            }
            delete task;
            return result;
        }

        void svc_end() override
        {
            const long long ns = timer_work.elapsed_ns().count();
            agg_->publish(static_cast<std::size_t>(idx_), ns);
            // spdlog::info("[Timer] Worker#{} total: {}", idx_, timer_work.result());
        }

    private:
        std::shared_ptr<Timings> agg_;
        int idx_;
    };

    struct Collector : ff::ff_node_t<TaskResult, void>
    {
        TimerClass timer_collect;
        explicit Collector(const std::string &outpath) : out(outpath, std::ios::binary)
        {
            if (!out)
                throw std::runtime_error("Collector: cannot open output");
        }
        int svc_init() override
        {
            // spdlog::info("[init] Collector tid={} cpu={}", get_tid(), sched_getcpu());
            return 0;
        }
        void *svc(TaskResult *result) override
        {
            TimerScope ts(timer_collect);
            // if the received result is part of segment we change to mode
            saw_spill = saw_spill || result->spill;
            if (result->kind == TaskResult::Kind::InMemBatch)
            {
                // IN-MEMORY mode: track wall-clock parallel time
                auto &seg = segments[result->segment_id];
                if (!seg.timing_started)
                {
                    seg.parallel_timer.start();
                    seg.timing_started = true;
                }
                seg.slices.push_back(SortedTask{result->items, result->segment_id, result->slice_index});
                seg.total_cpu_time_ns += result->sort_time_ns;

                // Check if segment complete (DEGREE slices received)
                if (seg.slices.size() == DEGREE)
                {
                    seg.parallel_timer.stop();
                    // spdlog::info("[Workers] Segment {} sort time: {}",
                    //              result->segment_id,
                    //              TimerClass::humanize_ns(seg.total_cpu_time_ns));
                    segments.erase(result->segment_id);
                }

                // take the items to perform final k-way merge
                inmem_batches.emplace_back(result->items);
                // spdlog::info("[Collector] Received in-memory batch, total batches: {}", inmem_batches.size());
            }
            else // TaskResult::Kind::SortedSlice
            {
                // OOC mode: track wall-clock parallel time
                SortedTask sorted_slice{result->items, result->segment_id, result->slice_index};
                auto &seg = segments[result->segment_id];
                if (!seg.timing_started)
                {
                    seg.parallel_timer.start();
                    seg.timing_started = true;
                }
                seg.slices.push_back(sorted_slice);
                seg.total_cpu_time_ns += result->sort_time_ns;

                // spdlog::info("[Collector] Segment {} slice {} received ({}/{} slices)",
                //              result->segment_id, result->slice_index, seg.slices.size(), DEGREE);

                // When segment complete (DEGREE slices received), write once
                if (seg.slices.size() == DEGREE)
                {
                    seg.parallel_timer.stop();
                    // spdlog::info("[Workers] Segment {} sort time: {}",
                    //              result->segment_id,
                    //              TimerClass::humanize_ns(seg.total_cpu_time_ns));

                    // spdlog::info("[Collector] Segment {} complete ({} slices) - Writing to disk",
                    //              result->segment_id, seg.slices.size());
                    std::string run_path = flush_segment_to_disk(seg.slices);
                    run_paths.push_back(run_path);
                    // spdlog::info("[Collector] Segment {} written -> {}", result->segment_id, run_path);
                    segments.erase(result->segment_id); // Free memory immediately
                    // spdlog::info("[Collector] Segment {} removed from memory", result->segment_id);
                }
            }
            delete result;
            return GO_ON;
        }
        void svc_end() override
        {
            {
                TimerScope ts(timer_collect);

                if (!saw_spill)
                {
                    struct CurrentItem
                    {
                        size_t b, i;
                        uint64_t key;
                    };
                    struct Comparer
                    {
                        bool operator()(const CurrentItem &a, const CurrentItem &b) const { return a.key > b.key; }
                    };
                    std::priority_queue<CurrentItem, std::vector<CurrentItem>, Comparer> pq;
                    for (size_t b = 0; b < inmem_batches.size(); ++b)
                        if (!inmem_batches[b]->empty())
                            pq.push(CurrentItem{b, 0, (*inmem_batches[b])[0].key});

                    size_t written = 0;
                    while (!pq.empty())
                    {
                        auto c = pq.top();
                        pq.pop();
                        const Item &it = (*inmem_batches[c.b])[c.i];
                        write_record(out, it.key, it.payload);
                        ++written;
                        size_t ni = c.i + 1;
                        if (ni < inmem_batches[c.b]->size())
                            pq.push(CurrentItem{c.b, ni, (*inmem_batches[c.b])[ni].key});
                    }
                    for (auto *p : inmem_batches)
                        delete p;
                    inmem_batches.clear();
                    // spdlog::info("Collector(inmem): wrote {} records -> {}", written, DATA_OUTPUT);
                }
                else
                {
                    // Handle any incomplete segments at end of pipeline
                    if (!segments.empty())
                    {
                        // size_t incomplete_count = segments.size();
                        // spdlog::info("[Collector] Processing {} incomplete segments", incomplete_count);
                        for (auto &[seg_id, seg] : segments)
                        {
                            if (!seg.slices.empty())
                            {
                                // spdlog::info("[Collector] Flushing incomplete segment {} ({} slices)",
                                //              seg_id, seg.slices.size());
                                std::string run_path = flush_segment_to_disk(seg.slices);
                                run_paths.push_back(run_path);
                            }
                        }
                        segments.clear();
                    }

                    if (run_paths.empty())
                    {
                        spdlog::warn("Collector(ooc): no runs produced");
                        // still timed, no early return
                    }
                    else
                    {
                        // spdlog::info("[Collector] Performing final k-way merge of {} runs", run_paths.size());
                        std::vector<std::unique_ptr<TempReader>> readers;
                        readers.reserve(run_paths.size());
                        for (auto &p : run_paths)
                            readers.push_back(std::make_unique<TempReader>(p));
                        struct HeapNode
                        {
                            uint64_t key;
                            size_t run_index;
                        };
                        struct Cmp
                        {
                            bool operator()(const HeapNode &a, const HeapNode &b) const { return a.key > b.key; }
                        };
                        std::priority_queue<HeapNode, std::vector<HeapNode>, Cmp> heap;
                        for (size_t r = 0; r < readers.size(); ++r)
                            if (!readers[r]->eof)
                                heap.push(HeapNode{readers[r]->key, r});

                        size_t written = 0;
                        while (!heap.empty())
                        {
                            auto h = heap.top();
                            heap.pop();
                            auto &rd = *readers[h.run_index];
                            write_record(out, rd.key, rd.payload);
                            ++written;
                            rd.advance();
                            if (!rd.eof)
                                heap.push(HeapNode{rd.key, h.run_index});
                        }
                        // spdlog::info("Collector(ooc): wrote {} records -> {}", written, DATA_OUTPUT);

                        for (auto &p : run_paths)
                        {
                            std::error_code ec;
                            std::filesystem::remove(p, ec);
                        }
                        run_paths.clear();
                    }
                }
                out.flush();
                out.close();
            }

            g_collector_time = timer_collect.result();
            // spdlog::info("[Timer] Collector: {}", g_collector_time);
        }

    private:
        std::ofstream out;
        bool saw_spill = false;
        std::vector<std::vector<Item> *> inmem_batches;
        std::vector<std::string> run_paths;

        // Coordinator state: accumulate slices by segment_id
        struct SegmentInfo
        {
            std::vector<SortedTask> slices;
            TimerClass parallel_timer;
            bool timing_started = false;
            long long total_cpu_time_ns = 0;
        };
        std::map<size_t, SegmentInfo> segments;
    };

}
// ---------- main ----------
int main(int argc, char **argv)
{
    using namespace ff_farm;
    TimerClass main_program_timer;
    std::shared_ptr<Timings> timings; // Declare outside try block for final report
    try
    {
        TimerScope main_program_time_scope(main_program_timer);
        // spdlog::info("==> FASTFLOW FARM implementation with MEMORY_CAP <==");
        parse_cli_and_set(argc, argv);
        // spdlog::info("==> Input: {} <==", DATA_INPUT);
        // spdlog::info("==> Workers: {} <==", WORKERS);
        // spdlog::info("==> DISTRIBUTION_CAP: {} MiB <==", DISTRIBUTION_CAP / (1024 * 1024));
        // spdlog::info("==> MEMORY_CAP: {:.2f} GiB <==", MEMORY_CAP / (1024.0 * 1024.0 * 1024.0));
        timings = std::make_shared<Timings>(WORKERS);
        Emitter emitter(DATA_INPUT, WORKERS);
        Collector collector(DATA_OUTPUT);

        ff::ff_Farm<Task, TaskResult> farm([&]()
                                           {
            std::vector<std::unique_ptr<ff::ff_node>> W;
            W.reserve(WORKERS);
            for (uint64_t i=0;i<WORKERS;++i)
                W.push_back(std::make_unique<Worker>(timings, static_cast<int>(i)));
            return W; }());
        farm.add_emitter(emitter);
        farm.add_collector(collector);
        farm.set_scheduling_ondemand(DEGREE);

        if (farm.run_and_wait_end() < 0)
        {
            spdlog::error("Farm execution failed");
            return EXIT_FAILURE;
        }
        // spdlog::info("==> Completed FARM → {}", DATA_OUTPUT);
        // spdlog::info("==> [Timer] Workers sum: {}", timings->total_str());
    }
    catch (const std::exception &e)
    {
        spdlog::error("==> X Operation failed: {} X <==", e.what());
        return EXIT_FAILURE;
    }
    // spdlog::info("[Timer] Main Program total: {}", main_program_timer.result());
    // spdlog::info("==> Completed successfully -> {} <==", DATA_OUTPUT);
    report.TOTAL_TIME = main_program_timer.result();
    if (timings)
        report.WORKING_TIME = timings->total_str();
    if (INPUT_BYTES > MEMORY_CAP)
        report.METHOD = "MEMORY_OOC";
    spdlog::info("M: {} | R: {} | PS: {} | W: {} | DC:{}MiB | ET: {} | WT: {} | CT: {} | TT: {}",
                 report.METHOD, report.RECORDS, report.PAYLOAD_SIZE, report.WORKERS,
                 DISTRIBUTION_CAP / IN_MB, g_emitter_time, report.WORKING_TIME, g_collector_time, report.TOTAL_TIME);
    return EXIT_SUCCESS;
}
