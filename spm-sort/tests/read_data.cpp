#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include "record.hpp"
#include "spdlog/spdlog.h"
std::string DATA_STREAM = "../data/records.bin";
int main()
{
    std::ifstream stream_in(DATA_STREAM, std::ios::binary);
    if (!stream_in)
    {
        spdlog::error("Cannot open '../data/tests/data_records.bin' to perform read");
        return EXIT_FAILURE;
    }
    uint64_t key;
    std::vector<uint8_t> payload;
    int index = 0;
    while (recordHeader::read_record(stream_in, key, payload))
    {
        std::cout << "rec#" << index++
                  << " key=" << key
                  << " len=" << payload.size()
                  << " payload=" << std::string(payload.begin(), payload.end())
                  << "\n";
    }
    return EXIT_SUCCESS;
}