#pragma once
#include "data_structure.hpp"
#include "spdlog/spdlog.h"
#include <iostream>
#include <cstdint>
#include <vector>
#include <string>

namespace common
{
    struct Report
    {
        uint64_t input_size_gb{0};
        uint64_t memory_gb{0};
        uint64_t workers{0};
        std::string summary{""};
    };
    // calculate stream size to decide where(in/out bound of memory) to process the data
    uint64_t estimate_stream_size();
    // give a summary based on resources
    Report get_summary(uint64_t input_size, uint64_t memory_gb, uint64_t workers);
    // load all the within memory
    void load_all_data_in_memory(std::vector<Item> &items, std::ifstream &in);
    // writing chunks inside ../data/temp/
    void write_temp_chunk(std::string &temp_chunk_out_path, const std::vector<Item> &temp_item_chunk);
}