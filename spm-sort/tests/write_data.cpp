#include <fstream>
#include <random>
#include <vector>
#include <iostream>
#include "record.hpp"
#include "spdlog/spdlog.h"

int main(int argc, char *argv[])
{
    /**
     * take input from the terminal for number of records to write;
     * default is 10;
     * max(u_int16_t) is 65535;
     */
    if (argc > 2)
    {
        spdlog::error("Not accepting more than 2 arguments");
        return EXIT_FAILURE;
    }

    const u_int16_t SIZE = argv[1] ? std::stoul(argv[1]) : 10;

    std::ofstream stream_out("../data/tests/data_records.bin", std::ios::binary);
    if (!stream_out)
    {
        spdlog::error("cannot open data_records.bin to perform write");
        return EXIT_FAILURE;
    }

    std::mt19937_64 seed(42);
    std::uniform_int_distribution<uint64_t> key_dist(0, 1000);
    std::uniform_int_distribution<int> len_dist(8, 16);
    std::uniform_int_distribution<int> char_dist('A', 'Z');

    for (int i = 0; i < SIZE; ++i)
    {
        const uint64_t key = key_dist(seed);
        const uint32_t len = static_cast<uint32_t>(len_dist(seed));
        std::vector<uint8_t> payload(len); // length between 8 and 16
        for (auto &b : payload)
            b = static_cast<uint8_t>(char_dist(seed));
        recordHeader::write_record(stream_out, key, payload);
        spdlog::info("written record: iteration{} key={}, len={}", i, key, len);
    }
    std::cout << "Data generation is complete: ../data/tests/data_records.bin\n";
    return EXIT_SUCCESS;
}