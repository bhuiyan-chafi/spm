#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <string>
#include "record.hpp"
#include "spdlog/spdlog.h"

const std::string DATA_OUT_STREAM = "../data/output.bin";
std::string DATA_IN_STREAM{""};
std::string DATA_STREAM{""};

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 4)
    {
        spdlog::error(" <{}> <how many records to pint> <input | output> < if->input : input_file_path >", argv[0]);
        throw std::runtime_error("Example: ./read_data 100 input ../data/rec_1M_256.bin");
    }

    const std::uint64_t records_to_print = std::stoull(argv[1]);
    const std::string which = argv[2];

    if (which == "input")
    {
        if (!argv[3])
        {
            spdlog::error("If you are printing INPUT data, you have to provide file path. Example: ../data/rec_1M_256.bin");
            return EXIT_FAILURE;
        }
        DATA_IN_STREAM = argv[3];
        DATA_STREAM = DATA_IN_STREAM;
    }
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