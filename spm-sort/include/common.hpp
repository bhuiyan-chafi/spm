#pragma once
#include "data_structure.hpp"
#include <iostream>
#include <cstdint>
#include <vector>

namespace common
{
    // calculate stream size to decide where(in/out bound of memory) to process the data
    uint64_t estimate_stream_size();
    // load all the within memory
    void load_all_data_in_memory(std::vector<Item> &items, std::ifstream &in);
    // writing chunks inside ../data/temp/
    void write_temp_chunk(std::string &temp_chunk_out_path, const std::vector<Item> &temp_item_chunk);
}