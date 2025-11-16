#pragma once
#include "ff/ff.hpp"
#include "timer.hpp"
#include "spdlog/spdlog.h"
#include "compact_payload_test.hpp"

#include <mutex>
#include <queue>
#include <deque>
#include <memory>
#include <random>
#include <vector>
#include <atomic>
#include <string>
#include <fstream>
#include <utility>
#include <cstdint>
#include <optional>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <sys/syscall.h>
#include <condition_variable>

/** ----------------- CONSTANTS and GLOBALS ----------------- */
const std::string DATA_OUTPUT{"../data/output.bin"};
const std::string DATA_TMP_DIR = "../data/tmp/";
// default 1 GB
const uint64_t IN_GB{1024UL * 1024UL * 1024UL};
const uint64_t IN_MB{1024UL * 1024UL};
const uint64_t IN_KB{1024UL};
const uint16_t SEEDER_SIZE{42};
inline std::string DATA_INPUT{""};
inline uint64_t WORKERS{0};
inline uint64_t MEMORY_CAP = IN_GB;
inline uint64_t PAYLOAD_MAX{0};
inline uint64_t RECORDS{0};
inline uint64_t INPUT_BYTES{0};
inline uint64_t DISTRIBUTION_CAP{0};
struct Report
{
    std::string METHOD;
    std::string RECORDS;
    std::string PAYLOAD_SIZE;
    uint64_t WORKERS;
    std::string WORKING_TIME;
    std::string TOTAL_TIME;
};
inline Report report;
inline unsigned long get_tid()
{
    return static_cast<unsigned long>(::syscall(SYS_gettid));
}

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
 *
 * @struct Node: represents in memory chunk items, used in OOC_OMP.hpp
 */
struct Node
{
    std::uint64_t key;
    std::size_t sub_range, offset;
};

struct Record
{
    uint64_t key;
    uint32_t len;
};

struct Item
{
    uint64_t key;
    CompactPayload payload; // payload bytes
};
/**
 *  Reading just one record: [u64 key][u32 len][len bytes payload]
 */
bool read_record(std::istream &stream_in, uint64_t &key_out,
                 CompactPayload &payload_out);
struct TempReader
{
    std::ifstream in;
    bool eof = false;
    uint64_t key = 0;
    CompactPayload payload;

    explicit TempReader(const std::string &path) : in(path, std::ios::binary)
    {
        if (!in)
            throw std::runtime_error("cannot open run for read: " + path);
        advance(); // prefetch first record
    }

    void advance()
    {
        if (!read_record(in, key, payload))
        {
            eof = true;
            // Drop any reserved capacity to avoid lingering large buffers
            payload = CompactPayload();
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
 * -------------- CLI and other processors --------------
 */
void parse_cli(int argc, char **argv);
/**
 *  Writing just one record: [u64 key][u32 len][len bytes payload]
 */
void write_record(std::ostream &stream_out, uint64_t key, const CompactPayload &payload);

// load data in memory
void load_all_data_in_memory(std::vector<Item> &items, std::ifstream &in);

// writing intermediate slices
void write_temp_slice(const std::string &temp_slice_out_path, const std::vector<Item> &temp_item_slice);
/**
 * ----------- Helper Functions -----------
 * details are in ./main.cpp
 */
// estimate stream size
uint64_t estimate_stream_size();

// sequential sort : in-memory
void sort_in_memory_test();
// sequential sort: out of memory bound
void sort_out_of_core_test();