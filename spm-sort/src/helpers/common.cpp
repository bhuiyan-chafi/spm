#include "data_structure.hpp"
#include "record.hpp"
#include "constants.hpp"
#include "spdlog/spdlog.h"

#include <iostream>
#include <fstream>
#include <filesystem>

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
     * load data: in memory or in chunks
     */
    void load_all_data_in_memory(std::vector<Item> &items, std::ifstream &in)
    {
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
