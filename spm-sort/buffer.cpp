#include "record.hpp"
#include "constants.hpp"
#include "data_structure.hpp"
#include "helper_ff.hpp"
#include "spdlog/spdlog.h"
#include "ff/ff.hpp"

#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <utility>
#include <cstdlib>

using u64 = std::uint64_t;
namespace fs = std::filesystem;

struct VerificationTask
{
    size_t chunk_id{};
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
            spdlog::error("Emitter: cannot open {}", out_path);
            throw std::runtime_error("Emitter failed to open output stream");
        }
        if (worker_count <= 0)
        {
            spdlog::error("Emitter: invalid worker count {}", worker_count);
            throw std::runtime_error("Emitter requires at least one worker");
        }
    }

    VerificationTask *svc(VerificationTask *) override
    {
        spdlog::info("Emitter: preparing {} chunk(s) for verification", ranges.size());
        for (size_t idx = 0; idx < ranges.size(); ++idx)
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
            task->records.reserve(static_cast<size_t>(expected));

            for (u64 j = 0; j < expected; ++j)
            {
                u64 key;
                std::vector<uint8_t> payload;
                if (!recordHeader::read_record(out, key, payload))
                {
                    spdlog::error("Emitter: unexpected end of {} while loading chunk {}", DATA_OUT_STREAM, idx);
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

            const unsigned int target_worker = static_cast<unsigned int>(next_worker);
            next_worker = (next_worker + 1) % static_cast<unsigned int>(worker_count);

            if (!this->ff_send_out(task.release(), static_cast<int>(target_worker)))
            {
                spdlog::error("Emitter: failed to dispatch chunk {} (target worker {})", idx, target_worker);
                throw std::runtime_error("Emitter failed to dispatch chunk");
            }
            spdlog::info("Emitter: chunk {} assigned to worker {}", idx, target_worker);
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
    unsigned int next_worker{0};
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
        spdlog::info("Worker {}: starting record verification for chunk {}",
                     worker_id, task_ptr->chunk_id);

        auto release_task_memory = [&task_ptr]()
        {
            for (auto &item : task_ptr->records)
                std::vector<uint8_t>().swap(item.payload);
            task_ptr->records.clear();
            task_ptr->records.shrink_to_fit();
        };

        if (task_ptr->records.empty())
        {
            spdlog::info("Worker {}: chunk {} empty, nothing to check", worker_id, task_ptr->chunk_id);
            release_task_memory();
            return this->GO_ON;
        }

        if (task_ptr->has_prev_last)
        {
            const u64 first_key = task_ptr->records.front().key;
            if (first_key < task_ptr->prev_last_key)
            {
                spdlog::error("Worker {}: chunk {} fails boundary check (prev_last={} first={})",
                              worker_id, task_ptr->chunk_id, task_ptr->prev_last_key, first_key);
                release_task_memory();
                throw std::runtime_error("Output stream is not globally sorted");
            }
        }

        for (size_t i = 1; i < task_ptr->records.size(); ++i)
        {
            if (task_ptr->records[i - 1].key > task_ptr->records[i].key)
            {
                spdlog::error("Worker {}: chunk {} fails local sortedness at position {} ({} > {})",
                              worker_id, task_ptr->chunk_id, i - 1,
                              task_ptr->records[i - 1].key, task_ptr->records[i].key);
                release_task_memory();
                throw std::runtime_error("Chunk is not sorted in non-decreasing order");
            }
        }
        spdlog::info("Worker {}: chunk {} passes sortedness check",
                     worker_id, task_ptr->chunk_id);

        std::ifstream input(DATA_IN_STREAM, std::ios::binary);
        if (!input)
        {
            spdlog::error("Worker {}: cannot open input stream {}", worker_id, DATA_IN_STREAM);
            release_task_memory();
            throw std::runtime_error("Failed to open input stream for verification");
        }

        std::vector<bool> matched(task_ptr->records.size(), false);
        size_t matched_count = 0;
        u64 key = 0;
        std::vector<uint8_t> payload;

        while (recordHeader::read_record(input, key, payload))
        {
            for (size_t i = 0; i < task_ptr->records.size(); ++i)
            {
                if (matched[i])
                    continue;
                const auto &candidate = task_ptr->records[i];
                if (candidate.key == key && candidate.payload == payload)
                {
                    matched[i] = true;
                    ++matched_count;
                    std::vector<uint8_t>().swap(task_ptr->records[i].payload);
                    break;
                }
            }

            if (matched_count == task_ptr->records.size())
                break;
        }

        if (matched_count != task_ptr->records.size())
        {
            spdlog::error("Worker {}: chunk {} mismatches input stream (matched {} of {})",
                          worker_id, task_ptr->chunk_id, matched_count, task_ptr->records.size());
            release_task_memory();
            throw std::runtime_error("Output records do not match input");
        }

        spdlog::info("Worker {}: chunk {} verified against input", worker_id, task_ptr->chunk_id);

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
        const auto input_size = fs::file_size(DATA_IN_STREAM);
        const auto output_size = fs::file_size(DATA_OUT_STREAM);
        if (input_size != output_size)
        {
            spdlog::error("Size mismatch: input={} bytes, output={} bytes", input_size, output_size);
            return EXIT_FAILURE;
        }
        spdlog::info("Input and output files have the same size: {} bytes", input_size);
    }
    catch (const std::exception &ex)
    {
        spdlog::error("Filesystem error: {}", ex.what());
        return EXIT_FAILURE;
    }

    std::ifstream out(DATA_OUT_STREAM, std::ios::binary);
    if (!out)
    {
        spdlog::error("Cannot open {}", DATA_OUT_STREAM);
        return EXIT_FAILURE;
    }

    u64 total_items = 0;
    while (true)
    {
        u64 key;
        std::vector<uint8_t> payload;
        if (!recordHeader::read_record(out, key, payload))
            break;
        ++total_items;
    }
    spdlog::info("Total Items: {}", total_items);

    const int WORKERS = ff_common::detect_workers();
    if (WORKERS <= 0)
    {
        spdlog::error("No workers detected.");
        return EXIT_FAILURE;
    }
    spdlog::info("Workers: {}", WORKERS);

    const size_t chunk_factor = 3;
    const size_t total_chunks = static_cast<size_t>(WORKERS) * chunk_factor;
    spdlog::info("Range configuration: workers={} chunk_factor={} total_chunks={}",
                 WORKERS, chunk_factor, total_chunks);

    std::vector<std::pair<u64, u64>> ranges;
    ranges.reserve(total_chunks);

    const u64 n = total_items;
    const u64 q = total_chunks > 0 ? (n / static_cast<u64>(total_chunks)) : n;
    const u64 r = total_chunks > 0 ? (n % static_cast<u64>(total_chunks)) : 0;

    u64 start = 0;
    for (size_t i = 0; i < total_chunks; ++i)
    {
        const u64 len = q + (static_cast<u64>(i) < r ? 1 : 0);
        const u64 end = start + len;
        ranges.emplace_back(start, end);
        start = end;
    }

    spdlog::info("Record index ranges [start, end):");
    for (size_t i = 0; i < ranges.size(); ++i)
    {
        spdlog::info("Range {} -> [{} , {})", i, ranges[i].first, ranges[i].second);
    }

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
            spdlog::error("Verification farm failed");
            return EXIT_FAILURE;
        }
        spdlog::info("Verification completed successfully for {} record(s)", total_items);
    }
    catch (const std::exception &ex)
    {
        spdlog::error("Verification failed: {}", ex.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
