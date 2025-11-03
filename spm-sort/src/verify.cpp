#include "record.hpp"
#include "constants.hpp"
#include "data_structure.hpp"
#include "helper_ff.hpp"
#include "spdlog/spdlog.h"
#include "ff/ff.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

using u64 = std::uint64_t;
namespace fs = std::filesystem;
static uint64_t TOTAL_INPUT_RECORDS{0};
namespace
{
    constexpr u64 FNV_OFFSET_BASIS = 1469598103934665603ULL;
    constexpr u64 FNV_PRIME = 1099511628211ULL;

    inline u64 fnv1a64(const std::uint8_t *data, std::size_t len) noexcept
    {
        u64 hash = FNV_OFFSET_BASIS;
        for (std::size_t i = 0; i < len; ++i)
        {
            hash ^= static_cast<u64>(data[i]);
            hash *= FNV_PRIME;
        }
        return hash;
    }

    inline u64 payload_hash(const std::vector<std::uint8_t> &payload) noexcept
    {
        if (payload.empty())
            return FNV_OFFSET_BASIS;
        return fnv1a64(payload.data(), payload.size());
    }

    std::atomic<u64> global_output_hash{0};

    u64 compute_input_payload_hash()
    {
        spdlog::info("==> READ, COUNT, CREATE_HASH of Records from {} <==", DATA_IN_STREAM);

        std::ifstream in(DATA_IN_STREAM, std::ios::binary);
        if (!in)
        {
            spdlog::error("==X Cannot open input stream {} X==", DATA_IN_STREAM);
            throw std::runtime_error("Failed to open input stream");
        }

        auto start_time = std::chrono::steady_clock::now();
        u64 sum = 0;
        u64 key = 0;
        std::vector<std::uint8_t> payload;
        u64 processed = 0;

        while (recordHeader::read_record(in, key, payload))
        {
            sum += payload_hash(payload);
            ++processed;
            if (processed % 10'000'000 == 0)
                spdlog::info("Input hashing progress: {} records", processed);
        }

        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
        spdlog::info("==> Input payload hash: {} | calculated all records in {:.2f}s <==",
                     sum, elapsed);
        TOTAL_INPUT_RECORDS = processed;
        spdlog::info("TOTAL_INPUT_RECORDS: {}", TOTAL_INPUT_RECORDS);
        return sum;
    }
}

struct VerificationTask
{
    std::size_t chunk_id{};
    std::pair<u64, u64> range{};
    std::vector<Item> records;
    bool has_prev_last{false};
    u64 prev_last_key{0};
};

class VerificationEmitter : public ff::ff_node_t<VerificationTask>
{
public:
    VerificationEmitter(const std::string &out_path,
                        std::vector<std::pair<u64, u64>> ranges_in,
                        int workers)
        : out(out_path, std::ios::binary),
          ranges(std::move(ranges_in)),
          worker_count(workers)
    {
        if (!out)
        {
            spdlog::error("==X Emitter: cannot open {} X==", out_path);
            throw std::runtime_error("Emitter failed to open output stream");
        }
        if (worker_count <= 0)
        {
            spdlog::error("==X Emitter: invalid worker count {} X==", worker_count);
            throw std::runtime_error("Emitter requires at least one worker");
        }
    }

    VerificationTask *svc(VerificationTask *) override
    {
        spdlog::info("Emitter: preparing {} chunk(s) for verification", ranges.size());
        for (std::size_t idx = 0; idx < ranges.size(); ++idx)
        {
            auto [start, end] = ranges[idx];
            if (end <= start)
            {
                spdlog::info("Emitter: chunk {} has no records, skipping", idx);
                continue;
            }

            auto task = std::make_unique<VerificationTask>();
            task->chunk_id = idx;
            task->range = {start, end};
            task->has_prev_last = has_last_key;
            task->prev_last_key = last_key;

            const u64 expected = end - start;
            task->records.reserve(static_cast<std::size_t>(expected));

            spdlog::info("Emitter: loading chunk {} → [{} , {}) ({} records)", idx, start, end, expected);

            for (u64 j = 0; j < expected; ++j)
            {
                u64 key;
                std::vector<std::uint8_t> payload;
                if (!recordHeader::read_record(out, key, payload))
                {
                    spdlog::error("==X Emitter: unexpected end of {} while loading chunk {} X==", DATA_OUT_STREAM, idx);
                    throw std::runtime_error("Emitter failed to read expected record");
                }
                task->records.push_back(Item{key, std::move(payload)});
            }

            if (!task->records.empty())
            {
                last_key = task->records.back().key;
                has_last_key = true;
            }

            spdlog::info("Emitter: emitted chunk {} covering [{} , {}) with {} record(s)",
                         idx, start, end, task->records.size());

            if (!this->ff_send_out(task.release()))
            {
                spdlog::error("==X Emitter: failed to dispatch chunk {} X==", idx);
                throw std::runtime_error("Emitter failed to dispatch chunk");
            }
        }
        spdlog::info("Emitter: all chunks dispatched");
        return this->EOS;
    }

private:
    std::ifstream out;
    std::vector<std::pair<u64, u64>> ranges;
    bool has_last_key{false};
    u64 last_key{0};
    int worker_count{0};
};

class VerificationWorker : public ff::ff_node_t<VerificationTask, void>
{
public:
    explicit VerificationWorker(int id) : worker_id(id) {}

    void *svc(VerificationTask *task) override
    {
        if (!task)
            return this->GO_ON;

        std::unique_ptr<VerificationTask> task_ptr(task);
        const auto [start, end] = task_ptr->range;
        spdlog::info("Worker {}: checking chunk {} covering [{} , {}) with {} record(s)",
                     worker_id, task_ptr->chunk_id, start, end, task_ptr->records.size());

        auto release_task_memory = [&task_ptr]()
        {
            for (auto &item : task_ptr->records)
                std::vector<std::uint8_t>().swap(item.payload);
            task_ptr->records.clear();
            task_ptr->records.shrink_to_fit();
        };

        if (task_ptr->records.empty())
        {
            spdlog::info("Worker {}: chunk {} empty, nothing to check", worker_id, task_ptr->chunk_id);
            release_task_memory();
            return this->GO_ON;
        }

        u64 local_hash = payload_hash(task_ptr->records.front().payload);

        if (task_ptr->has_prev_last)
        {
            const u64 first_key = task_ptr->records.front().key;
            if (first_key < task_ptr->prev_last_key)
            {
                spdlog::error("==X Worker {}: chunk {} fails boundary check (prev_last={} first={}) X==",
                              worker_id, task_ptr->chunk_id, task_ptr->prev_last_key, first_key);
                release_task_memory();
                throw std::runtime_error("Output stream is not globally sorted");
            }
        }

        for (std::size_t i = 1; i < task_ptr->records.size(); ++i)
        {
            if (task_ptr->records[i - 1].key > task_ptr->records[i].key)
            {
                spdlog::error("==X Worker {}: chunk {} fails local sortedness at position {} ({} > {}) X==",
                              worker_id, task_ptr->chunk_id, i - 1,
                              task_ptr->records[i - 1].key, task_ptr->records[i].key);
                release_task_memory();
                throw std::runtime_error("Chunk is not sorted in non-decreasing order");
            }
            local_hash += payload_hash(task_ptr->records[i].payload);
        }
        spdlog::info("Worker {}: chunk {} passes sortedness check",
                     worker_id, task_ptr->chunk_id);

        spdlog::info("Worker {}: chunk {} payload hash contribution = 0x{:016x}",
                     worker_id, task_ptr->chunk_id, local_hash);

        global_output_hash.fetch_add(local_hash, std::memory_order_relaxed);

        release_task_memory();
        return this->GO_ON;
    }

private:
    int worker_id;
};

int main()
{
    try
    {
        const u64 input_hash = compute_input_payload_hash();
        global_output_hash.store(0, std::memory_order_relaxed);
        try
        {
            const auto input_size = fs::file_size(DATA_IN_STREAM);
            const auto output_size = fs::file_size(DATA_OUT_STREAM);
            if (input_size != output_size)
            {
                spdlog::error("==X Size mismatch: input={} bytes, output={} bytes X==", input_size, output_size);
                return EXIT_FAILURE;
            }
            spdlog::info("Input and output files have the same size: {} bytes", input_size);
        }
        catch (const std::exception &ex)
        {
            spdlog::error("==X Filesystem error: {} X==", ex.what());
            return EXIT_FAILURE;
        }

        std::ifstream out(DATA_OUT_STREAM, std::ios::binary);
        if (!out)
        {
            spdlog::error("==X Cannot open {} X==", DATA_OUT_STREAM);
            return EXIT_FAILURE;
        }

        u64 total_items = 0;
        while (true)
        {
            u64 key;
            std::vector<std::uint8_t> payload;
            if (!recordHeader::read_record(out, key, payload))
                break;
            ++total_items;
        }
        spdlog::info("==> TOTAL_OUTPUT_RECORDS: {}", total_items);
        if (TOTAL_INPUT_RECORDS != total_items)
        {
            spdlog::error("==X ABORTED: Total Number of RECORDs don't match X==");
            return EXIT_FAILURE;
        }
        const int WORKERS = ff_numCores();
        if (WORKERS <= 0)
        {
            spdlog::error("==X No workers detected X==");
            return EXIT_FAILURE;
        }
        spdlog::info("==> WORKERS: {}", WORKERS);

        std::vector<std::pair<u64, u64>> ranges;
        ranges.reserve(static_cast<std::size_t>(WORKERS));

        const u64 n = total_items;
        const u64 q = (WORKERS > 0) ? (n / static_cast<u64>(WORKERS)) : n;
        const u64 r = (WORKERS > 0) ? (n % static_cast<u64>(WORKERS)) : 0;

        u64 start = 0;
        for (int i = 0; i < WORKERS; ++i)
        {
            const u64 len = q + (static_cast<u64>(i) < r ? 1 : 0);
            const u64 end = start + len;
            ranges.emplace_back(start, end);
            start = end;
        }

        spdlog::info("Record index ranges [start, end):");
        for (int i = 0; i < WORKERS; ++i)
            spdlog::info("Worker {} -> [{} , {})", i, ranges[i].first, ranges[i].second);

        try
        {
            VerificationEmitter emitter(DATA_OUT_STREAM, std::move(ranges), WORKERS);
            ff::ff_Farm<VerificationTask, void> farm(
                [&]()
                {
                    std::vector<std::unique_ptr<ff::ff_node>> workers;
                    workers.reserve(WORKERS);
                    for (int i = 0; i < WORKERS; ++i)
                        workers.push_back(std::make_unique<VerificationWorker>(i));
                    return workers;
                }());

            farm.add_emitter(emitter);
            farm.remove_collector();
            farm.set_scheduling_ondemand();

            if (farm.run_and_wait_end() < 0)
            {
                spdlog::error("==X Verification farm failed X==");
                return EXIT_FAILURE;
            }
            spdlog::info("==>Verification completed successfully for {} record(s)", total_items);
        }
        catch (const std::exception &ex)
        {
            spdlog::error("==X Verification failed: {} X==", ex.what());
            return EXIT_FAILURE;
        }

        const u64 output_hash = global_output_hash.load(std::memory_order_relaxed);
        spdlog::info("Output payload hash: 0x{:016x}", output_hash);
        if (output_hash != input_hash)
        {
            spdlog::error("==X Payload hash mismatch! input=0x{:016x} output=0x{:016x} X==", input_hash, output_hash);
            return EXIT_FAILURE;
        }
        spdlog::info("==> Payload hash matches INPUT={} : OUTPUT={}", input_hash, output_hash);
    }
    catch (const std::exception &ex)
    {
        spdlog::error("==X Verification aborted: {} X==", ex.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
