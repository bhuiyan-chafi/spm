#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <string>
#include "record.hpp"
#include "constants.hpp"
#include "spdlog/spdlog.h"

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
    {
        std::cerr << "Usage: " << argv[0] << " <records_to_print> [input|output]\n";
        return 1;
    }

    const std::uint64_t records_to_print = std::stoull(argv[1]);
    const std::string which = (argc == 3) ? std::string(argv[2]) : std::string("output");

    std::string DATA_STREAM;
    if (which == "input")
        DATA_STREAM = DATA_IN_STREAM;
    else if (which == "output")
        DATA_STREAM = DATA_OUT_STREAM;
    else
    {
        spdlog::error("Second arg must be 'input' or 'output' (got '{}')", which);
        return 1;
    }

    std::ifstream in(DATA_STREAM, std::ios::binary);
    if (!in)
    {
        spdlog::error("Cannot open '{}'", DATA_STREAM);
        return 1;
    }

    std::uint64_t key;
    std::vector<std::uint8_t> payload;
    std::uint64_t printed = 0;

    while (printed < records_to_print && recordHeader::read_record(in, key, payload))
    {
        std::cout << "rec#" << printed
                  << " \tkey=" << key
                  << " \tlen=" << payload.size()
                  << "\"\n";
        ++printed;
    }
    return 0;
}