/**
 * @file seq_sort.cpp
 * Author: ASM CHAFIULLAH BHUIYAN, M.Sc. in Computer Science and Network, University of Pisa, Italy
 * Contact: a.bhuiyan@studenti.unipi.it
 * Created on: October 2025
 * Project: Project-1(Merge Sort) problem in Parallel and Distributed Systems course,
 * University of Pisa, Italy)
 * You are free to use, modify, and distribute this code for educational purposes.
 */
#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include "record.hpp"
#include "spdlog/spdlog.h"
#include <queue>

/**
 * Constants:
 * @param DATA_IN_STREAM is the input data stream file path
 * @param DATA_OUT_STREAM is the output data stream file path
 * @param DATA_TMP_DIR is the temporary directory for intermediate files
 */

const std::string DATA_IN_STREAM = "../data/records.bin";
const std::string DATA_OUT_STREAM = "../data/sorted_records.bin";
const std::string DATA_TMP_DIR = "../data/tmp/";
const uint64_t MEMORY_CAP = 2'147'483'648ULL; // 2 GiB
/**
 * -------------------------------- Data Structures --------------------------------
 * @struct Item: represents a data item with a key and payload
 * @param key : 64-bit unsigned integer key
 * @param payload : vector of bytes representing the payload
 *
 * @struct TempReader: encapsulates the logic for reading from a sorted run file
 * @param in : input file stream for the run
 * @param eof : boolean flag indicating end-of-file
 * @param key : current key being read
 * @param payload : current payload being read
 * @brief advance(): reads the next record from the run file
 * we will create Readers == size_of_temp_record_paths, each reader reads from its own run file
 *
 * @struct HeapNode: represents a node in the min-heap used for k-way merging
 * @param key : 64-bit unsigned integer key
 * @param run_idx : index of the run this record came from
 * @brief operator> : comparison operator for min-heap ordering
 * No payload is stored here; payloads are kept in the TempReader to avoid copying.
 */
struct Item
{
    uint64_t key;
    std::vector<uint8_t> payload; // payload bytes
};

struct TempReader
{
    std::ifstream in;
    bool eof = false;
    uint64_t key = 0;
    std::vector<uint8_t> payload;

    explicit TempReader(const std::string &path) : in(path, std::ios::binary)
    {
        if (!in)
            throw std::runtime_error("cannot open run for read: " + path);
        advance(); // prefetch first record
    }

    void advance()
    {
        if (!recordHeader::read_record(in, key, payload))
        {
            eof = true;
        }
    }
};

struct HeapNode
{
    uint64_t key;
    size_t temp_run_index; // which run this record came from
    // no payload here; we keep payload in the TempReader to avoid copying
    bool operator>(const HeapNode &other) const { return key > other.key; }
};
/**
 * -------------------------------- Utility Functions --------------------------------
 * @brief estimate_stream_size(): estimates the size of the input data stream in bytes
 * @return total size of the data stream in bytes
 *
 * @brief write_temp_chunk : writes a temporary chunk of sorted items to a binary file
 * @param items : vector of sorted items to write
 * @return void
 */
inline uint64_t estimate_stream_size()
{
    std::ifstream in(DATA_IN_STREAM, std::ios::binary);
    uint64_t total_stream_size = 0ULL;
    while (true)
    {
        uint64_t key;
        std::vector<uint8_t> payload;
        if (!recordHeader::read_record(in, key, payload))
        {
            spdlog::info("Finished loading records into memory.");
            break; // EOF
        }
        total_stream_size += sizeof(uint64_t) + sizeof(uint32_t) + payload.size();
    }
    return total_stream_size;
}

inline void write_temp_chunk(std::string &temp_chunk_out_path, const std::vector<Item> &temp_item_chunk)
{
    std::ofstream out(temp_chunk_out_path, std::ios::binary);
    if (!out)
        throw std::runtime_error("Data stream error: cannot open input/output file.");
    for (const auto &item : temp_item_chunk)
    {
        recordHeader::write_record(out, item.key, item.payload);
    }
}
/**
 * -------------------------------- Sequential Sort Function --------------------------------
 * Sorting in Memory Function: sort_in_memory(), and the stream is for sure < MEMORY_CAP
 * @return true if sorting is successful, false otherwise
 */
static bool sort_in_memory()
{
    std::ifstream in(DATA_IN_STREAM, std::ios::binary);
    std::ofstream out(DATA_OUT_STREAM, std::ios::binary);
    if (!in || !out)
    {
        spdlog::error("Aborting execution from sort_in_memory().....");
        throw std::runtime_error("Data stream error: cannot open input/output file.");
    }
    std::vector<Item> items;
    spdlog::info("Phase: loading records into memory...");
    while (true)
    {
        uint64_t key;
        std::vector<uint8_t> payload;
        if (!recordHeader::read_record(in, key, payload))
        {
            spdlog::info("Finished loading records into memory.");
            break; // EOF
        }
        items.push_back(Item{key, std::move(payload)});
    }
    spdlog::info("Phase: sorting {} records in memory...", items.size());
    std::sort(items.begin(), items.end(),
              [](const Item &a, const Item &b)
              { return a.key < b.key; });

    spdlog::info("Phase: writing sorted records to output stream...");
    for (const auto &it : items)
    {
        recordHeader::write_record(out, it.key, it.payload);
    }
    return true;
}

/**
 * Sorting Out-of-Core Function: sort_out_of_core()
 * @return true if sorting is successful, false otherwise
 * Note: External (single-thread) merge sort: run generation + k-way merge
 */
static void sort_out_of_core()
{
    // ------------------ Phase A: make sorted runs -------------------
    std::ifstream in(DATA_IN_STREAM, std::ios::binary);
    std::ofstream out(DATA_OUT_STREAM, std::ios::binary);
    if (!in || !out)
        throw std::runtime_error("Data stream error: cannot open input/output file.");

    spdlog::info("Phase A: Since input doesn't fit in memory we are calculating how much memory we can use...");
    spdlog::info("Available memory size: {} GiB", MEMORY_CAP / (1024 * 1024 * 1024));
    std::vector<std::string> temp_record_paths;
    std::vector<Item> temp_items;
    uint64_t current_stream_size_inBytes = 0ULL;
    uint8_t current_stream_index = 0;
    uint64_t current_item_size = 0ULL;

    spdlog::info("Starting reading stream and creating sorted runs...");
    while (true)
    {
        uint64_t key;
        std::vector<uint8_t> payload;
        if (!recordHeader::read_record(in, key, payload))
        {
            /**
             * which means there are no items left to read,
             * and current_stream_size_inBytes < available_mem_bytes.
             */
            if (!temp_items.empty())
            {
                spdlog::info("Performing sort and write on tail part.....");
                std::sort(temp_items.begin(), temp_items.end(),
                          [](const Item &a, const Item &b)
                          { return a.key < b.key; });
                std::string temp_chunk_out_path = DATA_TMP_DIR + "run_" + std::to_string(current_stream_index) + ".bin";
                write_temp_chunk(temp_chunk_out_path, temp_items);
                temp_record_paths.push_back(temp_chunk_out_path);
                temp_items.clear();
                spdlog::info("Completed writing sorted chunk{} to temporary file{}", current_stream_index, temp_chunk_out_path);
            }
            break;
        }

        /**
         * we keep reading records until we fill up the available memory
         * calculate the size of the record to be added
         * the last sizeof(uint32_t) is a safeguard for overheads: metadata, alignment, etc.
         */
        current_item_size = sizeof(uint64_t) + sizeof(uint32_t) + payload.size() + sizeof(uint32_t);
        /**
         * check if adding this item would exceed MEMORY_CAP
         * if so we consider the current chunk full and proceed to sort and write it out
         * we always be on safe guard about the memory limit
         */
        if (current_stream_size_inBytes + current_item_size > MEMORY_CAP)
        {
            spdlog::info("We have saturated the memory cap and we will write perform sort and write on chunk{}...", current_stream_index);
            std::sort(temp_items.begin(), temp_items.end(),
                      [](const Item &a, const Item &b)
                      { return a.key < b.key; });
            std::string temp_chunk_out_path = DATA_TMP_DIR + "run_" + std::to_string(current_stream_index++) + ".bin";
            write_temp_chunk(temp_chunk_out_path, temp_items);
            temp_record_paths.push_back(temp_chunk_out_path);
            temp_items.clear();
            current_stream_size_inBytes = 0ULL;
            spdlog::info("Completed writing sorted chunk{} to temporary file{}", current_stream_index - 1, temp_chunk_out_path);
        }
        // the data was read and adding it would have saturated the memory cap, so we add it to the current chunk
        temp_items.push_back(Item{key, std::move(payload)});
        current_stream_size_inBytes += current_item_size;
    }
    /**
     *  ------------------ Phase B: Perform k-way merge -------------------
     *  ------------- accumulate chunks and produce one file --------------
     */
    std::vector<std::unique_ptr<TempReader>> readers;
    readers.reserve(temp_record_paths.size());
    // one TempReader per temp file
    for (const auto &record_path : temp_record_paths)
        readers.push_back(std::make_unique<TempReader>(record_path));
    /**
     * By default, std::priority_queue is a max-heap.
     * We need a min-heap for k-way merging, so we use std::greater<HeapNode>.
     * This will allow us to always extract the smallest key among the current heads of each run.
     * a > b means “a has a larger key”.
     * greater<HeapNode> uses that to keep the smallest key on top.
     */
    std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> heap;
    /**
     * initialize the heap with the first record from each run
     * we push only if the run is not empty (not EOF)
     * This sets up the initial state for the k-way merge process.
     */
    for (size_t i = 0; i < readers.size(); ++i)
    {
        if (!readers[i]->eof)
            heap.push(HeapNode{readers[i]->key, i});
    }
    while (!heap.empty())
    {
        auto top = heap.top();
        heap.pop();
        auto &current_reader = *readers[top.temp_run_index];

        // write current record
        recordHeader::write_record(out, current_reader.key, current_reader.payload);

        // advance that run
        current_reader.advance();
        if (!current_reader.eof)
        {
            heap.push(HeapNode{current_reader.key, top.temp_run_index});
        }
    }

    // cleanup temp runs
    for (const auto &temp_record_path : temp_record_paths)
    {
        std::remove(temp_record_path.c_str());
    }
}
int main()
{
    /**
     * we will first estimate the size of the input data stream. If the size is within the MEMORY_CAP, we will proceed with in-memory sorting using std::sort. Otherwise, we will proceed with external sorting.
     */
    uint64_t stream_size = 0ULL;
    spdlog::info("Estimating input data stream size in bytes...");
    stream_size = estimate_stream_size();
    /**
     * if the estimated stream size is less than or equal to the memory limit, we will sort in memory. Otherwise, we will perform out-of-core sorting.
     */
    spdlog::info("-------------- Starting Sequential Sort --------------");
    try
    {
        if (stream_size <= MEMORY_CAP)
        {
            spdlog::info("Estimated input data stream size: {} Bytes", stream_size);
            spdlog::info("Input data stream fits in memory. Proceeding with in-memory sort...");
            sort_in_memory();
            spdlog::info("In-memory sorting completed. Sorted data written to {}", DATA_OUT_STREAM);
        }
        else
        {
            spdlog::info("Estimated input data stream size: {} GiB", stream_size / (1024 * 1024 * 1024));
            spdlog::warn("Input data stream exceeds memory limit");
            spdlog::info("Proceeding with out-of-core sort...");
            sort_out_of_core();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}