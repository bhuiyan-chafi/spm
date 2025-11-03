#pragma once
#include "spdlog/spdlog.h"
#include <iostream>
#include <cstdint>
#include <unistd.h>
#include <sys/syscall.h>
/** ----------------- CONSTANTS ----------------- */
std::string DATA_INPUT{""};
std::string DATA_OUTPUT{"../data/output.bin"};
const std::string DATA_TMP_DIR = "../data/tmp/";
// default 1 GB
const uint64_t IN_GB{1024UL * 1024UL * 1024UL};
uint64_t MEMORY_CAP = IN_GB;
uint64_t WORKERS{0};

/**
 * -------------- CLI arguments processor --------------
 */
inline void parse_cli_and_set(int argc, char **argv)
{

    if (argc != 5)
    {
        spdlog::info("Execution: ./program <1: records in Million> <256: payload_max> <workers> <32: memory>");
        spdlog::error("Program accepts exactly 5 parameters, please read the instructions properly!");
        throw std::runtime_error("");
    }
    std::string records = argv[1];
    std::string payload = argv[2];
    uint64_t workers = std::stoull(argv[3]);
    uint64_t memory = std::stoull(argv[4]);
    DATA_INPUT = "../data/rec_" + records + "M_" + payload + ".bin";
    MEMORY_CAP = memory * IN_GB;
    // if worker is mistakenly assigned too low
    if (workers < 1)
        workers = 1;
    WORKERS = workers;

    spdlog::info("Parameters -> DATA_INPUT={}, MEMORY_CAP={} GiB, WORKERS={}", DATA_INPUT, memory, WORKERS);
}

inline unsigned long get_tid()
{
    return static_cast<unsigned long>(::syscall(SYS_gettid));
}
