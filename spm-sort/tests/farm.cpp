/**
 * @author ASM CHAFIULLAH BHUIYAN
 */
#include "main.hpp"
#include "timer.hpp"
#include "record.hpp"
#include "common.hpp"
#include "spdlog/spdlog.h"
#include "data_structure.hpp"

#include <ff/ff.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * -------------- Utility Functions --------------
 * @param chunk_ranges: hold the ranges of data for a worker
 */

static inline std::vector<std::pair<size_t, size_t>>
chunk_ranges(size_t n, size_t parts)
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
 * @param RunPath: if we had to spill the data into primary chunks
 */
struct Task
{
    std::vector<Item> *items;
    bool spill;
    size_t segment_id;
    size_t slice_index;
};
struct TaskResult
{
    enum class Kind
    {
        InMemBatch,
        RunPath
    } kind;
    std::vector<Item> *items = nullptr;
    std::string *path = nullptr;
    bool spill = false;
    size_t segment_id = 0, slice_index = 0;
};

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
            spdlog::info("[init] Emitter tid={} cpu={}", get_tid(), sched_getcpu());
            if (!in)
                throw std::runtime_error("Emitter: cannot open input");
            return 0;
        }
        Task *svc(Task *) override
        {
            TimerScope ts(timer_emit);
            auto input_buffer = std::make_unique<ITEMS>();
            uint64_t accumulator = 0;
            bool exceeded = false;
            while (true)
            {
                uint64_t key;
                std::vector<uint8_t> payload;
                if (!recordHeader::read_record(in, key, payload))
                    break; // EOF
                const uint64_t record_size = sizeof(uint64_t) + sizeof(uint32_t) + payload.size();
                if (accumulator + record_size > MEMORY_CAP)
                {

                    exceeded = true;
                    // we are breaking the while loop but we still have items left to read
                    break;
                }
                input_buffer->push_back(Item{key, std::move(payload)});
                accumulator += record_size;
            }
            // resetting the accumulator
            accumulator = 0;
            // if input_buffer < MEMORY_CAP : we loaded everything in memory
            if (!exceeded)
            {
                spdlog::info("Emitter: in-memory path, total items={}", input_buffer->size());
                auto ranges = chunk_ranges(input_buffer->size(), Workers ? Workers : 1);
                for (size_t i = 0; i < ranges.size(); ++i)
                {
                    auto [L, R] = ranges[i];
                    auto *slice = new ITEMS;
                    slice->reserve(R - L);
                    for (size_t j = L; j < R; ++j)
                        slice->push_back(std::move((*input_buffer)[j]));
                    ff_send_out(new Task{slice, /*spill=*/false, 0, i});
                }
                return EOS;
            }

            // we broke the loop with one input_buffer loaded
            spdlog::info("Emitter: OOC path, streaming segments at ~MEMORY_CAP");
            size_t segment_id = 0;
            // receives the first input_buffer before breaking first read and receives the remaining slices
            auto emit_segment = [&](std::unique_ptr<ITEMS> seg)
            {
                auto ranges = chunk_ranges(seg->size(), Workers ? Workers : 1);
                for (size_t i = 0; i < ranges.size(); ++i)
                {
                    auto [L, R] = ranges[i];
                    auto *slice = new ITEMS;
                    slice->reserve(R - L);
                    for (size_t j = L; j < R; ++j)
                        slice->push_back(std::move((*seg)[j]));
                    // each worker gets one Task emitted by the Emitter, with on-demand-scheduling we process have processed it
                    ff_send_out(new Task{slice, /*spill=*/true, segment_id, i});
                }
                ++segment_id;
            };
            // sending the first loaded input_buffer before we broke the reading
            emit_segment(std::move(input_buffer));

            // we are not reading again from the start, using the same input stream which starts from where it left
            while (true)
            {
                auto segment = std::make_unique<ITEMS>();
                while (true)
                {
                    uint64_t key;
                    std::vector<uint8_t> payload;
                    if (!recordHeader::read_record(in, key, payload))
                    {
                        if (!segment->empty())
                            emit_segment(std::move(segment));
                        return EOS;
                    }
                    const uint64_t record_size = sizeof(uint64_t) + sizeof(uint32_t) + payload.size();
                    if (accumulator + record_size > MEMORY_CAP && !segment->empty())
                    {
                        emit_segment(std::move(segment));
                        segment = std::make_unique<ITEMS>();
                        accumulator = 0;
                    }
                    segment->push_back(Item{key, std::move(payload)});
                    accumulator += record_size;
                }
            }
        }
        void svc_end() override { spdlog::info("[Timer] Emitter: {}", timer_emit.result()); }

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
            spdlog::info("[init] Worker tid={} cpu={}", get_tid(), sched_getcpu());
            std::filesystem::create_directories(DATA_TMP_DIR);
            return 0;
        }
        TaskResult *svc(Task *task) override
        {
            TimerScope ts(timer_work);

            auto *local_items = task->items;
            std::sort(local_items->begin(), local_items->end(),
                      [](const Item &a, const Item &b)
                      { return a.key < b.key; });

            auto *result = new TaskResult();
            result->spill = task->spill;
            result->segment_id = task->segment_id;
            result->slice_index = task->slice_index;
            // if it was within memory operation
            if (!task->spill)
            {
                result->kind = TaskResult::Kind::InMemBatch;
                // hand sorted slice to collector
                result->items = local_items;
            }
            else
            {
                static std::atomic<uint64_t> run_id{0};
                const auto id = run_id.fetch_add(1, std::memory_order_relaxed);
                const std::string path = DATA_TMP_DIR + "run_" + std::to_string(id) + ".bin";
                auto path_copy = path; // write_temp_chunk takes non-const ref
                common::write_temp_chunk(path_copy, *local_items);
                delete local_items;
                result->kind = TaskResult::Kind::RunPath;
                result->path = new std::string(path);
            }
            delete task;
            return result;
        }

        void svc_end() override
        {
            const long long ns = timer_work.elapsed_ns().count();
            agg_->publish(static_cast<std::size_t>(idx_), ns);
            spdlog::info("[Timer] Worker#{} total: {}", idx_, timer_work.result());
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
            spdlog::info("[init] Collector tid={} cpu={}", get_tid(), sched_getcpu());
            return 0;
        }
        void *svc(TaskResult *result) override
        {
            TimerScope ts(timer_collect);
            // if the received result is part of segment we change to mode
            saw_spill = saw_spill || result->spill;
            if (result->kind == TaskResult::Kind::InMemBatch)
            {
                // take the items to perform final k-way merge
                inmem_batches.emplace_back(result->items);
            }
            else
            {
                // take the total slices to merge
                run_paths.emplace_back(std::move(*result->path));
                delete result->path;
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
                        recordHeader::write_record(out, it.key, it.payload);
                        ++written;
                        size_t ni = c.i + 1;
                        if (ni < inmem_batches[c.b]->size())
                            pq.push(CurrentItem{c.b, ni, (*inmem_batches[c.b])[ni].key});
                    }
                    for (auto *p : inmem_batches)
                        delete p;
                    inmem_batches.clear();
                    spdlog::info("Collector(inmem): wrote {} records -> {}", written, DATA_OUTPUT);
                }
                else
                {
                    if (run_paths.empty())
                    {
                        spdlog::warn("Collector(ooc): no runs produced");
                        // still timed, no early return
                    }
                    else
                    {
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
                            recordHeader::write_record(out, rd.key, rd.payload);
                            ++written;
                            rd.advance();
                            if (!rd.eof)
                                heap.push(HeapNode{rd.key, h.run_index});
                        }
                        spdlog::info("Collector(ooc): wrote {} records -> {}", written, DATA_OUTPUT);

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

            spdlog::info("[Timer] Collector total: {}", timer_collect.result());
        }

    private:
        std::ofstream out;
        bool saw_spill = false;
        std::vector<std::vector<Item> *> inmem_batches;
        std::vector<std::string> run_paths;
    };

}
// ---------- main ----------
int main(int argc, char **argv)
{
    using namespace ff_farm;
    TimerClass main_program_timer;
    try
    {
        TimerScope main_program_time_scope(main_program_timer);
        spdlog::info("==> FASTFLOW FARM implementation with MEMORY_CAP <==");
        parse_cli_and_set(argc, argv);
        auto timings = std::make_shared<Timings>(WORKERS);
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
        farm.set_scheduling_ondemand(); // auto-scheduling; consider farm.set_scheduling_ondemand(4)

        if (farm.run_and_wait_end() < 0)
        {
            spdlog::error("Farm execution failed");
            return EXIT_FAILURE;
        }
        spdlog::info("==> Completed FARM → {}", DATA_OUTPUT);
        spdlog::info("==> [Timer] Workers sum: {}", timings->total_str());
    }
    catch (const std::exception &e)
    {
        spdlog::error("Aborted: {}", e.what());
        return EXIT_FAILURE;
    }
    spdlog::info("[Timer] Main Program total(has main thread drop delay): {}", main_program_timer.result());
}