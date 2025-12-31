#pragma once
#include <iostream>
#include <fstream>
#include "record.hpp"

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
            // Drop any reserved capacity to avoid lingering large buffers
            std::vector<uint8_t>().swap(payload);
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

struct Node
{
    std::uint64_t key;
    std::size_t sub_range, offset;
};