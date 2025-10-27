#include "helper.hpp"
#include "data_structure.hpp"
#include "record.hpp"
#include "constants.hpp"
#include "spdlog/spdlog.h"

#include <iostream>
#include <fstream>
#include <queue>
#include <filesystem>

#if defined(DEFAULT_MAX_THREADS)
static const size_t THREADS = DEFAULT_MAX_THREADS;
#elif defined(_OPENMP)
#include <omp.h>
static const size_t THREADS = omp_get_max_threads();
#else
static const size_t THREADS = 1;
#endif

namespace common
{
    /**
     * calculate stream size to decide where(in/out bound of memory) to process the data
     */
    uint64_t estimate_stream_size()
    {
        namespace fs = std::filesystem;
        std::error_code error_code;
        auto stream_size = fs::file_size(DATA_IN_STREAM, error_code);
        if (!error_code)
        {
            stream_size = static_cast<uint64_t>(stream_size);
            if (stream_size < SMALL_DATA_SIZE)
            {
                spdlog::info("INPUT DATA SIZE: {} MiB", stream_size / (1024 * 1024));
                return stream_size;
            }
            else
            {
                spdlog::info("INPUT DATA SIZE: {} GiB", stream_size / (1024 * 1024 * 1024));
                return stream_size;
            }
        }
        spdlog::error("==> X => Error in ESTIMATING_STREAM_SIZE.....");
        throw std::runtime_error("");
    }
    /**
     * writing chunks inside ../data/temp/
     */
    void write_temp_chunk(std::string &temp_chunk_out_path, const std::vector<Item> &temp_item_chunk)
    {
        std::ofstream out(temp_chunk_out_path, std::ios::binary);
        if (!out)
            throw std::runtime_error("Data stream error: cannot open input/output file.");
        for (const auto &item : temp_item_chunk)
        {
            recordHeader::write_record(out, item.key, item.payload);
        }
    }
}

namespace seq_sort
{
    /**
     * sorting in Memory Function: sort_in_memory(), and the stream is for sure < MEMORY_CAP
     */
    void sort_in_memory()
    {
        spdlog::info("==> PHASE: 3 -> Creating read/write data stream.....");
        std::ifstream in(DATA_IN_STREAM, std::ios::binary);
        std::ofstream out(DATA_OUT_STREAM, std::ios::binary);
        if (!in || !out)
        {
            spdlog::error("==> X => Error in in/out DATA_STREAM.....");
            throw std::runtime_error("");
        }
        std::vector<Item> items;
        spdlog::info("==> PHASE: 4 -> Load all DATA_STREAM in memory .....");
        while (true)
        {
            uint64_t key;
            std::vector<uint8_t> payload;
            if (!recordHeader::read_record(in, key, payload))
            {
                spdlog::info("==> PHASE: 4.1 -> Finished loading DATA_STREAM .....");
                break; // EOF
            }
            items.push_back(Item{key, std::move(payload)});
        }
        spdlog::info("Total INPUT quantity: {}", items.size());
        spdlog::info("==> PHASE: 5 -> Starting standard library sort.....");
        std::sort(items.begin(), items.end(),
                  [](const Item &a, const Item &b)
                  { return a.key < b.key; });

        spdlog::info("==> PHASE: 6 -> Writing results ....");
        for (const auto &it : items)
        {
            recordHeader::write_record(out, it.key, it.payload);
        }
    }
    /**
     * sort in chunks, out of memory capacity(MEMORY_CAP)
     */
    void sort_out_of_core()
    {
        spdlog::info("==> PHASE: 3 -> Creating reader+writer for data stream.....");
        std::ifstream in(DATA_IN_STREAM, std::ios::binary);
        std::ofstream out(DATA_OUT_STREAM, std::ios::binary);
        if (!in || !out)
        {
            spdlog::error("==> X => Error in in/out DATA_STREAM.....");
            throw std::runtime_error("");
        }
        spdlog::info("==> PHASE: 4 -> Setting local variables.....");
        std::vector<std::string> temp_record_paths;
        std::vector<Item> temp_items;
        uint64_t current_stream_size_inBytes = 0ULL;
        uint64_t current_stream_index = 0;
        uint64_t current_item_size = 0ULL;

        spdlog::info("==> PHASE: 5 -> Setting MEMORY_CAP as defined: {} GiB", MEMORY_CAP / (1024 * 1024 * 1024));
        spdlog::info("==> PHASE: 6 -> Start reading+loading DATA_STREAM.....");
        while (true)
        {
            uint64_t key;
            std::vector<uint8_t> payload;
            if (!recordHeader::read_record(in, key, payload))
            {
                spdlog::info("==> PHASE: 6.1 -> No more items in DATA_STREAM.....");
                if (!temp_items.empty())
                {
                    spdlog::info("==> PHASE: 7.{} -> Tail: Starting standard library sort for chunk_{}", current_stream_index, current_stream_index);
                    std::sort(temp_items.begin(), temp_items.end(),
                              [](const Item &a, const Item &b)
                              { return a.key < b.key; });
                    std::string temp_chunk_out_path = DATA_TMP_DIR + "run_" + std::to_string(current_stream_index) + ".bin";
                    common::write_temp_chunk(temp_chunk_out_path, temp_items);
                    temp_record_paths.push_back(temp_chunk_out_path);
                    spdlog::info("Total INPUT quantity: {}", temp_items.size());
                    temp_items.clear();
                    std::vector<Item>().swap(temp_items);
                    spdlog::info("==> PHASE: 7.{} -> Completed standard library sort for chunk_{}", current_stream_index, current_stream_index);
                }
                break;
            }
            current_item_size = sizeof(uint64_t) + sizeof(uint32_t) + payload.size() + sizeof(uint16_t);
            if ((current_stream_size_inBytes + current_item_size) > MEMORY_CAP)
            {
                spdlog::info("==> PHASE: 7.{} -> Starting standard library sort for chunk_{}", current_stream_index, current_stream_index);
                std::sort(temp_items.begin(), temp_items.end(),
                          [](const Item &a, const Item &b)
                          { return a.key < b.key; });
                std::string temp_chunk_out_path = DATA_TMP_DIR + "run_" + std::to_string(current_stream_index++) + ".bin";
                common::write_temp_chunk(temp_chunk_out_path, temp_items);
                temp_record_paths.push_back(temp_chunk_out_path);
                temp_items.clear();
                std::vector<Item>().swap(temp_items);
                current_stream_size_inBytes = 0ULL;
                spdlog::info("==> PHASE: 7.{} -> Completed standard library sort for chunk_{}", current_stream_index - 1, current_stream_index - 1);
            }
            temp_items.push_back(Item{key, std::move(payload)});
            current_stream_size_inBytes += current_item_size;
        }
        spdlog::info("==> PHASE: 7 -> Starting K-Way merge on data chunks.....");
        /**
         *  ------------------ Phase B: Perform k-way merge -------------------
         *  ------------- accumulate chunks and produce one file --------------
         */
        spdlog::info("==> PHASE: 8 -> Creating READERS for data chunks.....");
        std::vector<std::unique_ptr<TempReader>> readers;
        readers.reserve(temp_record_paths.size());
        for (const auto &record_path : temp_record_paths)
            readers.push_back(std::make_unique<TempReader>(record_path));
        std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> heap;
        for (uint64_t i = 0; i < readers.size(); ++i)
        {
            if (!readers[i]->eof)
                heap.push(HeapNode{readers[i]->key, i});
        }
        spdlog::info("==> PHASE: 9 -> Writing RECORDS from data chunks.....");
        while (!heap.empty())
        {
            auto top = heap.top();
            heap.pop();
            auto &current_reader = *readers[top.temp_run_index];
            recordHeader::write_record(out, current_reader.key, current_reader.payload);
            current_reader.advance();
            if (!current_reader.eof)
            {
                heap.push(HeapNode{current_reader.key, top.temp_run_index});
            }
        }
        spdlog::info("==> PHASE: 10 -> Cleaning up data chunks.....");
        for (const auto &temp_record_path : temp_record_paths)
        {
            std::remove(temp_record_path.c_str());
        }
    }
}

namespace omp_sort
{
    /**
     * sorting in Memory Function: sort_in_memory(), and the stream is for sure < MEMORY_CAP
     */
    void sort_in_memory_omp()
    {
        spdlog::info("==> PHASE: 3 -> Creating read/write data stream.....");
        std::ifstream in(DATA_IN_STREAM, std::ios::binary);
        std::ofstream out(DATA_OUT_STREAM, std::ios::binary);
        if (!in || !out)
        {
            spdlog::error("==> X => Error in in/out DATA_STREAM.....");
            throw std::runtime_error("");
        }
        std::vector<Item> items;
        spdlog::info("==> PHASE: 4 -> Load all DATA_STREAM in memory .....");
        while (true)
        {
            std::uint64_t key;
            std::vector<std::uint8_t> payload;
            if (!recordHeader::read_record(in, key, payload))
            {
                spdlog::info("==> PHASE: 4.1 -> Finished loading DATA_STREAM .....");
                break; // EOF
            }
            items.push_back(Item{key, std::move(payload)});
        }
        spdlog::info("==> PHASE: 5 -> Starting standard library sort with OMP.....");
        sort_run_parallel(items);
        spdlog::info("==> PHASE: 6 -> Writing results.....");
        for (const auto &it : items)
            recordHeader::write_record(out, it.key, it.payload);
        spdlog::info("==> PHASE: 6.1 -> Finished writing results.....");
    }
    void sort_run_parallel(std::vector<Item> &input)
    {
        spdlog::info("==> PHASE: 5.1 -> Checking input size against 2^18 .....");
        const uint64_t total_input_size = input.size();

        spdlog::info("Total INPUT quantity: {}", total_input_size);
        if (total_input_size < (1u << 18))
        {
            spdlog::info("Input is so small that we are performing a standard SORT.....");
            std::sort(input.begin(), input.end(), [](const Item &a, const Item &b)
                      { return a.key < b.key; });
            return;
        }

        spdlog::info("==> PHASE: 5.2 -> Creating chunks against THREADS = {}", THREADS);
        std::vector<uint64_t> chunks(THREADS + 1);
        for (uint64_t chunk_index = 0; chunk_index <= THREADS; ++chunk_index)
            chunks[chunk_index] = chunk_index * total_input_size / THREADS;

        spdlog::info("==> PHASE: 5.3 -> Performing SORT on CHUNKS.....");
#ifdef _OPENMP
#pragma omp parallel for schedule(static) num_threads(THREADS)
#endif
        for (uint64_t chunk_index = 0; chunk_index < THREADS; ++chunk_index)
        {
            auto Left = input.begin() + chunks[chunk_index];
            auto Right = input.begin() + chunks[chunk_index + 1];
            std::sort(Left, Right, [](const Item &a, const Item &b)
                      { return a.key < b.key; });
        }

        spdlog::info("==> PHASE: 5.4 -> Assigning temporary vector PARALLEL run.....");
        std::vector<Item> tmp;
        tmp.reserve(total_input_size);

        spdlog::info("==> PHASE: 5.5 -> Creating COMPARISON_QUEUE.....");
        auto comparison_key = [](const Node &a, const Node &b)
        { return a.key > b.key; };
        std::priority_queue<Node, std::vector<Node>, decltype(comparison_key)> comparison_queue(comparison_key);

        spdlog::info("==> PHASE: 5.6 -> Fill the HEAP.....");
        for (uint64_t chunk_index = 0; chunk_index < THREADS; ++chunk_index)
            if (chunks[chunk_index] < chunks[chunk_index + 1])
                comparison_queue.push(Node{input[chunks[chunk_index]].key, chunk_index, 0});

        spdlog::info("==> PHASE: 5.7 -> Perform the MERGE.....");
        while (!comparison_queue.empty())
        {
            auto node = comparison_queue.top();
            comparison_queue.pop();
            std::uint64_t base = chunks[node.sub_range];
            tmp.push_back(std::move(input[base + node.offset]));
            std::uint64_t next_i = node.offset + 1;
            if (base + next_i < chunks[node.sub_range + 1])
                comparison_queue.push(Node{input[base + next_i].key, node.sub_range, next_i});
        }
        spdlog::info("==> PHASE: 5.8 -> Swap TMP with INPUT received.....");
        input.swap(tmp);
    }
    /**
     * sort in chunks, out of memory capacity(MEMORY_CAP)
     */
    void sort_out_of_core_omp()
    {
        spdlog::info("==> PHASE: 3 -> Creating read/write data stream.....");
        std::ifstream in(DATA_IN_STREAM, std::ios::binary);
        std::ofstream out(DATA_OUT_STREAM, std::ios::binary);

        spdlog::info("==> PHASE: 4 -> Assigning local VARIABLES.....");
        std::vector<std::string> temp_record_paths;
        std::vector<Item> temp_items;
        uint64_t current_stream_size_inBytes = 0ULL;
        uint64_t current_stream_index = 0;
        uint64_t current_item_size = 0ULL;
        spdlog::info("==> PHASE: 5 -> Creating and loading chunks based on MEMORY_CAP: {} GiB", MEMORY_CAP / (1024UL * 1024UL * 1024UL));
        while (true)
        {
            uint64_t key;
            std::vector<uint8_t> payload;
            if (!recordHeader::read_record(in, key, payload))
            {
                if (!temp_items.empty())
                {
                    spdlog::info("Performing PARALLEL_SORT on the TAIL.....");
                    sort_run_parallel(temp_items);
                    std::string outp = DATA_TMP_DIR + "run_" + std::to_string(current_stream_index++) + ".bin";
                    common::write_temp_chunk(outp, temp_items);
                    temp_record_paths.push_back(outp);
                    spdlog::info("Total INPUT quantity: {}", temp_items.size());
                    temp_items.clear();
                    current_stream_size_inBytes = 0ULL;
                }
                break;
            }
            current_item_size = sizeof(uint64_t) + sizeof(uint32_t) + payload.size() + sizeof(uint32_t);

            if (current_stream_size_inBytes + current_item_size > MEMORY_CAP)
            {
                spdlog::info("MEMORY_CAP saturated: performing PARALLEL_SORT on chunk_{}", current_stream_index);
                // which runs the sort in parallel
                sort_run_parallel(temp_items);
                std::string temp_chunk_out_path = DATA_TMP_DIR + "run_" + std::to_string(current_stream_index++) + ".bin";
                common::write_temp_chunk(temp_chunk_out_path, temp_items);
                temp_record_paths.push_back(temp_chunk_out_path);
                temp_items.clear();
                current_stream_size_inBytes = 0ULL;
                spdlog::info("Completed writing sorted chunk_{}", current_stream_index - 1);
            }

            // the last item before last saturation of MEMORY_CAP
            temp_items.push_back(Item{key, std::move(payload)});
            current_stream_size_inBytes += current_item_size;
        }

        spdlog::info("==> PHASE: 6 -> Performing PARALLEL_MERGE on {} TEMP_RECORDS", temp_record_paths.size());
        parallel_grouped_merge(temp_record_paths, DATA_OUT_STREAM, DATA_TMP_DIR);

        spdlog::info("==> PHASE: 7 -> Clearing TEMP_RECORDS.....");
        for (const auto &p : temp_record_paths)
            std::remove(p.c_str());
    }
    void parallel_grouped_merge(const std::vector<std::string> &run_paths,
                                const std::string &final_out,
                                const std::string &tmp_dir)
    {
        // no more temp_files
        if (run_paths.empty())
        {
            std::ofstream(final_out, std::ios::binary);
            return;
        }
        // how many files and threads we have, then we distribute/schedule the load
        spdlog::info("==> PHASE: 6.1 -> Creating GROUPS to distribute/schedule TEMP_FILES.....");
        const int GROUPS = std::min<int>(std::max(1UL, THREADS), static_cast<int>(run_paths.size()));

        // best case scenario
        if (GROUPS <= 1)
        {
            merge_runs_to_file(run_paths, final_out);
            return;
        }

        // distributing/scheduling (round-robin)
        spdlog::info("==> PHASE: 6.1 -> Distributing/Scheduling GROUPS.....");
        std::vector<std::vector<std::string>> groups(GROUPS);
        for (std::uint64_t i = 0; i < run_paths.size(); ++i)
            groups[i % GROUPS].push_back(run_paths[i]);
        spdlog::info("==> PHASE: 6.2 -> Creating INTERMEDIATE groups and distributing/scheduling loads.....");
        std::vector<std::string> intermediates(GROUPS);
#ifdef _OPENMP
#pragma omp parallel for schedule(static) num_threads(THREADS)
#endif
        for (int group_index = 0; group_index < GROUPS; ++group_index)
        {
            intermediates[group_index] = tmp_dir + "inter_" + std::to_string(group_index) + ".bin";
            // sequential engine
            merge_runs_to_file(groups[group_index], intermediates[group_index]);
        }

        spdlog::info("==> PHASE: 6.3 -> Final SINGLE_MERGE of intermediates to form one TEMP_FILE of previous CHUNK.....");
        merge_runs_to_file(intermediates, final_out);
        spdlog::info("==> PHASE: 6.4 -> Removing INTERMEDIATE chunks.....");
        for (auto &p : intermediates)
            std::remove(p.c_str());
    }
    void merge_runs_to_file(const std::vector<std::string> &temp_paths,
                            const std::string &out_path)
    {
        std::ofstream out(out_path, std::ios::binary);
        if (!out)
            throw std::runtime_error("cannot open output for merge: " + out_path);
        spdlog::info("==> PHASE: 6.3.1 -> Creating READERS for Intermediate chunks.....");
        std::vector<std::unique_ptr<TempReader>> readers;
        readers.reserve(temp_paths.size());
        for (const auto &p : temp_paths)
            readers.push_back(std::make_unique<TempReader>(p));
        spdlog::info("==> PHASE: 6.3.1 -> Creating HEAPS for Intermediate chunks.....");
        std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> heap;
        for (std::uint64_t i = 0; i < readers.size(); ++i)
            if (!readers[i]->eof)
                heap.push(HeapNode{readers[i]->key, i});

        spdlog::info("==> PHASE: 6.3.1 -> Performing WRITE from the HEAP.....");
        while (!heap.empty())
        {
            auto top = heap.top();
            heap.pop();
            auto &rr = *readers[top.temp_run_index];

            recordHeader::write_record(out, rr.key, rr.payload);

            rr.advance();
            if (!rr.eof)
                heap.push(HeapNode{rr.key, top.temp_run_index});
        }
    }
}