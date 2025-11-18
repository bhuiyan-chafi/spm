#include "main.hpp"
#include <mpi.h>

/**
 * MPI + FastFlow Hybrid External Sort
 *
 * Architecture:
 * - MPI: Multi-node distribution (master distributes segments to worker nodes)
 * - FastFlow: Intra-node parallelism (each worker node runs a FastFlow farm)
 *
 * Master (rank 0):
 * - Reads input file, creates segments at DISTRIBUTION_CAP boundaries
 * - Dynamically distributes segments to idle worker nodes via MPI
 * - Collects sorted segments from workers
 * - Performs final k-way merge
 *
 * Workers (rank 1+):
 * - Receive segment via MPI
 * - Run FastFlow farm: Emitter slices → Workers sort → Collector merges
 * - Send sorted segment back to master via MPI
 */

namespace
{
    using Items = std::vector<Item>;

    // MPI Tags
    constexpr int TAG_WORK = 1;
    constexpr int TAG_RESULT = 2;
    constexpr int TAG_TERMINATE = 3;
    constexpr uint8_t TERMINATE_FLAG = 0xFF;

    // ==================== UTILITY FUNCTIONS ====================

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

    // ==================== DATA STRUCTURES ====================

    struct Task
    {
        std::vector<Item> *items;
        size_t slice_index;
    };

    struct TaskResult
    {
        std::vector<Item> *items;
        size_t slice_index;
    };

    struct WorkPackage
    {
        bool spill;
        uint64_t segment_id;
        Items items;
    };

    // ==================== MERGE FUNCTIONS ====================

    struct HeapItem
    {
        size_t slice_idx, item_idx;
        uint64_t key;
        bool operator>(const HeapItem &other) const { return key > other.key; }
    };

    // ==================== MPI COMMUNICATION ====================

    enum class MsgTag : int
    {
        WorkHeader = 1,
        WorkKey = 2,
        WorkLen = 3,
        WorkPayload = 4,
        ResultHeader = 5,
        ResultKey = 6,
        ResultLen = 7,
        ResultPayload = 8
    };

    struct MessageHeader
    {
        uint8_t spill;
        uint64_t segment_id;
        uint64_t item_count;
    };

    void send_header(int dest, const MessageHeader &header, MsgTag tag, MPI_Comm comm)
    {
        MPI_Send(const_cast<MessageHeader *>(&header),
                 sizeof(header),
                 MPI_BYTE,
                 dest,
                 static_cast<int>(tag),
                 comm);
    }

    MessageHeader receive_header(int source, MsgTag tag, MPI_Comm comm)
    {
        MessageHeader header{};
        MPI_Recv(&header,
                 sizeof(header),
                 MPI_BYTE,
                 source,
                 static_cast<int>(tag),
                 comm,
                 MPI_STATUS_IGNORE);
        return header;
    }

    void send_items(int dest, const Items &items, MsgTag key_tag, MsgTag len_tag, MsgTag payload_tag, MPI_Comm comm)
    {
        for (const auto &it : items)
        {
            MPI_Send(const_cast<uint64_t *>(&it.key), 1, MPI_UINT64_T, dest, static_cast<int>(key_tag), comm);
            uint32_t payload_len = static_cast<uint32_t>(it.payload.size());
            MPI_Send(&payload_len, 1, MPI_UINT32_T, dest, static_cast<int>(len_tag), comm);
            if (payload_len > 0)
            {
                MPI_Send(const_cast<uint8_t *>(it.payload.data()),
                         static_cast<int>(payload_len),
                         MPI_BYTE,
                         dest,
                         static_cast<int>(payload_tag),
                         comm);
            }
        }
    }

    void receive_items(int source,
                       Items &items,
                       std::size_t count,
                       MsgTag key_tag,
                       MsgTag len_tag,
                       MsgTag payload_tag,
                       MPI_Comm comm)
    {
        items.clear();
        items.reserve(count);
        for (std::size_t idx = 0; idx < count; ++idx)
        {
            uint64_t key = 0;
            MPI_Recv(&key, 1, MPI_UINT64_T, source, static_cast<int>(key_tag), comm, MPI_STATUS_IGNORE);
            uint32_t payload_len = 0;
            MPI_Recv(&payload_len, 1, MPI_UINT32_T, source, static_cast<int>(len_tag), comm, MPI_STATUS_IGNORE);
            CompactPayload payload;
            if (payload_len > 0)
            {
                payload.resize(payload_len);
                MPI_Recv(payload.data(),
                         static_cast<int>(payload_len),
                         MPI_BYTE,
                         source,
                         static_cast<int>(payload_tag),
                         comm,
                         MPI_STATUS_IGNORE);
            }
            items.push_back(Item{key, std::move(payload)});
        }
    }

    void send_work(int dest, const WorkPackage &work, MPI_Comm comm)
    {
        MessageHeader header{};
        header.spill = work.spill ? 1 : 0;
        header.segment_id = work.segment_id;
        header.item_count = work.items.size();
        send_header(dest, header, MsgTag::WorkHeader, comm);
        send_items(dest, work.items, MsgTag::WorkKey, MsgTag::WorkLen, MsgTag::WorkPayload, comm);
    }

    bool receive_work(int source, WorkPackage &work, MPI_Comm comm)
    {
        MessageHeader header = receive_header(source, MsgTag::WorkHeader, comm);
        if (header.spill == TERMINATE_FLAG && header.item_count == 0)
            return false;
        Items items;
        receive_items(source,
                      items,
                      header.item_count,
                      MsgTag::WorkKey,
                      MsgTag::WorkLen,
                      MsgTag::WorkPayload,
                      comm);
        work.spill = header.spill != 0;
        work.segment_id = header.segment_id;
        work.items = std::move(items);
        return true;
    }

    void send_result(int dest, const WorkPackage &result, MPI_Comm comm)
    {
        MessageHeader header{};
        header.spill = result.spill ? 1 : 0;
        header.segment_id = result.segment_id;
        header.item_count = result.items.size();
        send_header(dest, header, MsgTag::ResultHeader, comm);
        send_items(dest, result.items, MsgTag::ResultKey, MsgTag::ResultLen, MsgTag::ResultPayload, comm);
    }

    WorkPackage receive_result(MPI_Comm comm, int &source_rank)
    {
        MPI_Status status;
        MessageHeader header{};
        MPI_Recv(&header,
                 sizeof(header),
                 MPI_BYTE,
                 MPI_ANY_SOURCE,
                 static_cast<int>(MsgTag::ResultHeader),
                 comm,
                 &status);
        source_rank = status.MPI_SOURCE;

        Items items;
        receive_items(source_rank,
                      items,
                      header.item_count,
                      MsgTag::ResultKey,
                      MsgTag::ResultLen,
                      MsgTag::ResultPayload,
                      comm);

        WorkPackage result;
        result.spill = header.spill != 0;
        result.segment_id = header.segment_id;
        result.items = std::move(items);
        return result;
    }

    void send_terminate(int dest, MPI_Comm comm)
    {
        MessageHeader header{};
        header.spill = TERMINATE_FLAG;
        header.item_count = 0;
        send_header(dest, header, MsgTag::WorkHeader, comm);
    }

    // ==================== FASTFLOW NODES (Worker-side) ====================

    namespace ff_worker
    {
        struct Emitter : ff::ff_node_t<Task>
        {
            Emitter(Items *segment) : segment_(segment) {}

            Task *svc(Task *) override
            {
                auto ranges = slice_ranges(segment_->size(), DEGREE);
                // spdlog::info("[FF][Emitter] Slicing {} items into {} slices", segment_->size(), ranges.size());
                for (size_t i = 0; i < ranges.size(); ++i)
                {
                    auto [L, R] = ranges[i];
                    auto *slice = new Items;
                    slice->reserve(R - L);
                    for (size_t j = L; j < R; ++j)
                        slice->push_back(std::move((*segment_)[j]));
                    ff_send_out(new Task{slice, i});
                }
                return EOS;
            }

        private:
            Items *segment_;
        };

        struct Worker : ff::ff_node_t<Task, TaskResult>
        {
            Worker(std::shared_ptr<Timings> agg, int idx, TimerClass *external_timer = nullptr)
                : agg_(std::move(agg)), idx_(idx), external_timer_(external_timer) {}

            TaskResult *svc(Task *task) override
            {
                auto *local_items = task->items;
                // spdlog::info("[FF][Worker-{}] Sorting slice {} ({} items)", idx_, task->slice_index, local_items->size());

                TimerClass &timer = external_timer_ ? *external_timer_ : timer_work;
                timer.start();
                std::sort(local_items->begin(), local_items->end(),
                          [](const Item &a, const Item &b)
                          { return a.key < b.key; });
                timer.stop();
                // //spdlog::info("[Worker-{}] Completed slice_{} in {}",
                //              idx_, task->slice_index, timer.result());
                auto *result = new TaskResult{local_items, task->slice_index};
                delete task;
                return result;
            }

            void svc_end() override
            {
                // Only publish if using internal timer (not external)
                if (!external_timer_)
                {
                    const long long ns = timer_work.elapsed_ns().count();
                    agg_->publish(static_cast<std::size_t>(idx_), ns);
                }
                // spdlog::info("[FF][Worker-{}] Total: {}", idx_, external_timer_->result());
            }

        private:
            std::shared_ptr<Timings> agg_;
            int idx_;
            TimerClass *external_timer_; // If provided, use this instead of timer_work
            TimerClass timer_work;       // Internal timer (used when external_timer_ is null)
        };

        struct Collector : ff::ff_node_t<TaskResult, void>
        {
            Collector(Items *output) : output_(output) {}

            void *svc(TaskResult *result) override
            {
                slices_.push_back(result);
                return GO_ON;
            }

            void svc_end() override
            {
                // K-way merge all slices into output
                if (slices_.empty())
                    return;

                std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
                for (size_t s = 0; s < slices_.size(); ++s)
                    if (slices_[s]->items && !slices_[s]->items->empty())
                        heap.push(HeapItem{s, 0, (*slices_[s]->items)[0].key});

                output_->clear();
                output_->reserve(estimate_total_items());

                while (!heap.empty())
                {
                    auto current = heap.top();
                    heap.pop();
                    const Item &item = (*slices_[current.slice_idx]->items)[current.item_idx];
                    output_->push_back(item);

                    const size_t next_idx = current.item_idx + 1;
                    if (next_idx < slices_[current.slice_idx]->items->size())
                        heap.push(HeapItem{current.slice_idx, next_idx,
                                           (*slices_[current.slice_idx]->items)[next_idx].key});
                }

                // Free memory
                for (auto &slice : slices_)
                {
                    if (slice->items)
                    {
                        delete slice->items;
                        slice->items = nullptr;
                    }
                    delete slice;
                }
                slices_.clear();

                // spdlog::info("[FF][Collector] Merged {} slices -> {} items", slices_.size(), output_->size());
            }

        private:
            size_t estimate_total_items()
            {
                size_t total = 0;
                for (const auto &slice : slices_)
                    if (slice->items)
                        total += slice->items->size();
                return total;
            }

            Items *output_;
            std::vector<TaskResult *> slices_;
        };
    } // namespace ff_worker

    // ==================== MASTER NODE ====================

    void master_main(int world_size)
    {
        if (world_size < 2)
            throw std::runtime_error("MPI+FastFlow requires at least 2 processes");

        std::filesystem::create_directories(DATA_TMP_DIR);

        // Determine mode
        namespace fs = std::filesystem;
        std::error_code ec;
        const auto stream_bytes = fs::file_size(DATA_INPUT, ec);
        const bool fits_in_memory = (!ec) && stream_bytes <= MEMORY_CAP;

        // spdlog::info("[MPI][Master] Mode: {}, size={} bytes, MEMORY_CAP={} bytes",
        //  fits_in_memory ? "IN-MEMORY" : "OOC",
        //  stream_bytes,
        //  MEMORY_CAP);

        std::ifstream in(DATA_INPUT, std::ios::binary);
        if (!in)
            throw std::runtime_error("Master: cannot open input");

        std::queue<int> idle_workers;
        for (int rank = 1; rank < world_size; ++rank)
            idle_workers.push(rank);

        std::vector<std::unique_ptr<Items>> inmem_batches;
        std::vector<std::string> run_paths;
        uint64_t run_id = 0;

        uint64_t tasks_outstanding = 0;
        uint64_t segment_id = 0;

        auto read_and_send_segment = [&]() -> bool
        {
            static bool has_leftover = false;
            static uint64_t leftover_key;
            static CompactPayload leftover_payload;

            if (idle_workers.empty())
                return false;

            Items segment;
            segment.reserve(64'000);
            uint64_t accumulator = 0ULL;

            if (has_leftover)
            {
                const uint64_t record_size = sizeof(uint64_t) + sizeof(uint32_t) + leftover_payload.size();
                segment.push_back(Item{leftover_key, std::move(leftover_payload)});
                accumulator += record_size;
                has_leftover = false;
            }

            while (true)
            {
                uint64_t key;
                CompactPayload payload;
                if (!read_record(in, key, payload))
                {
                    // EOF
                    break;
                }

                const uint64_t record_size = sizeof(uint64_t) + sizeof(uint32_t) + payload.size();
                if (accumulator + record_size > DISTRIBUTION_CAP && !segment.empty())
                {
                    has_leftover = true;
                    leftover_key = key;
                    leftover_payload = std::move(payload);
                    break;
                }
                segment.push_back(Item{key, std::move(payload)});
                accumulator += record_size;
            }

            if (segment.empty())
                return false;

            int worker = idle_workers.front();
            idle_workers.pop();

            WorkPackage work{!fits_in_memory, segment_id++, std::move(segment)};
            send_work(worker, work, MPI_COMM_WORLD);
            tasks_outstanding++;
            // spdlog::info("[MPI][Master] Sent segment {} to worker {} ({} items)",
            //  work.segment_id, worker, work.items.size());
            return true;
        };

        // Send initial work
        while (read_and_send_segment())
            ;

        // Collect results and send more work
        while (tasks_outstanding > 0)
        {
            int source_rank = 0;
            WorkPackage result = receive_result(MPI_COMM_WORLD, source_rank);
            tasks_outstanding--;
            idle_workers.push(source_rank);

            // spdlog::info("[MPI][Master] Received result from rank {} -> segment {} ({} items)",
            //  source_rank, result.segment_id, result.items.size());

            if (result.spill)
            {
                // OOC mode: write segment immediately (already merged by worker's FastFlow)
                auto path = DATA_TMP_DIR + "mpi_run_" + std::to_string(run_id++) + ".bin";
                std::ofstream out(path, std::ios::binary);
                if (!out)
                    throw std::runtime_error("Cannot write: " + path);
                for (const auto &item : result.items)
                    write_record(out, item.key, item.payload);
                run_paths.push_back(std::move(path));
                // spdlog::info("[MPI][Master] Segment {} written -> {}", result.segment_id, run_paths.back());
            }
            else
            {
                inmem_batches.push_back(std::make_unique<Items>(std::move(result.items)));
            }

            // Try to send more work
            read_and_send_segment();
        }

        // Terminate workers
        for (int rank = 1; rank < world_size; ++rank)
            send_terminate(rank, MPI_COMM_WORLD);

        // Final merge
        if (!run_paths.empty())
        {
            report.METHOD = "MPI_OOC";
            // K-way merge of run files
            std::ofstream out(DATA_OUTPUT, std::ios::binary);
            if (!out)
                throw std::runtime_error("Cannot open output");

            std::vector<std::unique_ptr<TempReader>> readers;
            readers.reserve(run_paths.size());
            for (const auto &path : run_paths)
                readers.push_back(std::make_unique<TempReader>(path));

            struct HeapNode
            {
                uint64_t key;
                std::size_t run_index;
            };
            struct Cmp
            {
                bool operator()(const HeapNode &a, const HeapNode &b) const { return a.key > b.key; }
            };

            std::priority_queue<HeapNode, std::vector<HeapNode>, Cmp> heap;
            for (std::size_t i = 0; i < readers.size(); ++i)
                if (!readers[i]->eof)
                    heap.push(HeapNode{readers[i]->key, i});

            size_t written = 0;
            while (!heap.empty())
            {
                auto top = heap.top();
                heap.pop();
                auto &reader = *readers[top.run_index];
                write_record(out, reader.key, reader.payload);
                ++written;
                reader.advance();
                if (!reader.eof)
                    heap.push(HeapNode{reader.key, top.run_index});
            }

            // spdlog::info("[MPI][Master] Final merge complete: {} records", written);

            // Cleanup
            for (const auto &path : run_paths)
            {
                std::error_code ec;
                std::filesystem::remove(path, ec);
            }
        }
        else
        {
            report.METHOD = "MPI_INMEM";
            // In-memory final merge
            std::ofstream out(DATA_OUTPUT, std::ios::binary);
            if (!out)
                throw std::runtime_error("Cannot open output");

            struct HeapNode
            {
                std::size_t batch_index;
                std::size_t item_index;
                uint64_t key;
            };
            struct Cmp
            {
                bool operator()(const HeapNode &a, const HeapNode &b) const { return a.key > b.key; }
            };

            std::priority_queue<HeapNode, std::vector<HeapNode>, Cmp> heap;
            for (std::size_t b = 0; b < inmem_batches.size(); ++b)
                if (!inmem_batches[b]->empty())
                    heap.push(HeapNode{b, 0, (*inmem_batches[b])[0].key});

            size_t written = 0;
            while (!heap.empty())
            {
                auto node = heap.top();
                heap.pop();
                const Item &item = (*inmem_batches[node.batch_index])[node.item_index];
                write_record(out, item.key, item.payload);
                ++written;
                std::size_t next_idx = node.item_index + 1;
                if (next_idx < inmem_batches[node.batch_index]->size())
                    heap.push(HeapNode{node.batch_index, next_idx, (*inmem_batches[node.batch_index])[next_idx].key});
            }

            // spdlog::info("[MPI][Master] In-memory merge complete: {} records", written);
        }
    }

    // ==================== WORKER NODE ====================

    void worker_main(int rank)
    {
        // Create Timings once for entire worker lifetime, accumulates across all segments
        std::shared_ptr<Timings> timings = std::make_shared<Timings>(WORKERS);

        // Create persistent timers that accumulate across all farms/segments
        std::vector<TimerClass> worker_timers(WORKERS);
        uint64_t segments_processed = 0;

        while (true)
        {
            WorkPackage work;
            if (!receive_work(0, work, MPI_COMM_WORLD))
                break;

            // spdlog::info("[MPI][Worker-{}] Received segment {} ({} items)",
            //  rank, work.segment_id, work.items.size());

            // Run FastFlow farm on this segment
            Items result;
            {
                using namespace ff_worker;

                Emitter emitter(&work.items);
                Collector collector(&result);

                // Create workers that share references to persistent timers
                ff::ff_Farm<Task, TaskResult> farm([&]()
                                                   {
                    std::vector<std::unique_ptr<ff::ff_node>> W;
                    W.reserve(WORKERS);
                    for (uint64_t i = 0; i < WORKERS; ++i)
                        W.push_back(std::make_unique<Worker>(timings, static_cast<int>(i), &worker_timers[i]));
                    return W; }());

                farm.add_emitter(emitter);
                farm.add_collector(collector);
                farm.set_scheduling_ondemand(DEGREE);

                if (farm.run_and_wait_end() < 0)
                {
                    spdlog::error("[MPI][Worker-{}] FastFlow farm execution failed", rank);
                    throw std::runtime_error("FastFlow farm failed");
                }
            }

            segments_processed++;
            // spdlog::info("[MPI][Worker-{}] FastFlow farm complete, sending {} items back (segment {})",
            //  rank, result.size(), segments_processed);

            WorkPackage response{work.spill, work.segment_id, std::move(result)};
            send_result(0, response, MPI_COMM_WORLD);
        }

        // Publish accumulated timing from all persistent timers
        for (size_t i = 0; i < WORKERS; ++i)
        {
            const long long ns = worker_timers[i].elapsed_ns().count();
            timings->publish(i, ns);
        }

        // Send accumulated timing back to master
        uint64_t elapsed_ns = timings->total();
        MPI_Send(&elapsed_ns, 1, MPI_UINT64_T, 0, 999, MPI_COMM_WORLD);

        // spdlog::info("[MPI][Worker-{}] Finished: {} segments processed, total time: {}",
        //  rank, segments_processed, timings->total_str());
    }

} // namespace

// ==================== MAIN ====================

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int world_rank = 0;
    int world_size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    try
    {
        parse_cli_and_set(argc, argv);

        if (world_rank == 0)
        {
            TimerClass total_time;
            TimerClass total_worker_time;
            {
                TimerScope total_scope(total_time);
                master_main(world_size);
            }

            // Collect worker times from all workers
            uint64_t accumulated_worker_ns = 0;
            for (int rank = 1; rank < world_size; ++rank)
            {
                uint64_t worker_ns = 0;
                MPI_Recv(&worker_ns, 1, MPI_UINT64_T, rank, 999, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                accumulated_worker_ns += worker_ns;
            }
            total_worker_time.add_elapsed(std::chrono::nanoseconds(accumulated_worker_ns));

            // Set report values
            // report.WORKERS = world_size - 1; // Number of MPI workers
            report.WORKERS = WORKERS; // Number of threads not MPI workers
            report.WORKING_TIME = total_worker_time.result();
            report.TOTAL_TIME = total_time.result();

            // Final report matching ff_farm.cpp format
            spdlog::info("M: {} | R: {} | PS: {} | W: {} | DC:{}MiB | WT: {} | TT: {}",
                         report.METHOD,
                         report.RECORDS,
                         report.PAYLOAD_SIZE,
                         report.WORKERS,
                         DISTRIBUTION_CAP / IN_MB,
                         report.WORKING_TIME,
                         report.TOTAL_TIME);
        }
        else
        {
            worker_main(world_rank);
        }
    }
    catch (const std::exception &error)
    {
        spdlog::error("Rank {} aborted due to: {}", world_rank, error.what());
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
