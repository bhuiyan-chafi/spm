/**
 * @file record.hpp
 * Author: ASM CHAFIULLAH BHUIYAN, M.Sc. in Computer Science and Network, University of Pisa, Italy
 * Contact: a.bhuiyan@studenti.unipi.it
 * Created on: October 2025
 * Project: Project-1(Merge Sort) problem in Parallel and Distributed Systems course,
 * University of Pisa, Italy)
 * You are free to use, modify, and distribute this code for educational purposes.
 */
#include "main.hpp"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        throw std::invalid_argument("Usage: ./generator <PAYLOAD_MAX: 8~256> <RECORDS: 1M 5M 10M>");
        return EXIT_FAILURE;
    }
    TimerClass generation_time;
    parse_cli_and_set(argc, argv);
    // DATA_INPUT because we are writing records as INPUTs
    std::ofstream data_stream_out(DATA_INPUT, std::ios::binary);
    if (!data_stream_out)
    {

        throw std::runtime_error("Data stream out failed....");
        return EXIT_FAILURE;
    }
    {
        TimerScope gt(generation_time);
        // random data generation functions setup
        std::mt19937_64 seeder(SEEDER_SIZE);
        std::uniform_int_distribution<uint64_t> key_dist(0, std::numeric_limits<uint64_t>::max());
        std::uniform_int_distribution<int> len_dist(8, PAYLOAD_MAX);
        std::uniform_int_distribution<int> upper_or_lower(0, 1);
        std::uniform_int_distribution<int> upper_dist('A', 'Z');
        std::uniform_int_distribution<int> lower_dist('a', 'z');

        spdlog::info("==> Starting data generation with parameters len:{},size:{} <==", PAYLOAD_MAX, RECORDS);
        spdlog::info("-> Data generation started, please wait till it finishes");
        for (uint64_t index = 0; index < RECORDS; ++index)
        {
            const uint64_t key = key_dist(seeder);
            const uint32_t len = static_cast<uint32_t>(len_dist(seeder));
            std::vector<uint8_t> payload(len); // length between 8 and PAYLOAD_MAX
            for (auto &bytes : payload)
            {
                bytes = static_cast<std::uint8_t>(
                    upper_or_lower(seeder) ? lower_dist(seeder) : upper_dist(seeder));
            }
            write_record(data_stream_out, key, payload);
        }
    }
    spdlog::info("->[Timer] : Generated Random Data -> {}", generation_time.result());
    spdlog::info("==> Data Generation Completed <==");
    return EXIT_SUCCESS;
}
