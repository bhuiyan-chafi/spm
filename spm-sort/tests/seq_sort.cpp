/**
 * @file seq_sort.cpp
 * Author: ASM CHAFIULLAH BHUIYAN, M.Sc. in Computer Science and Network, University of Pisa, Italy
 * Contact: a.bhuiyan@studenti.unipi.it
 * Created on: October 2025
 * Project: Project-1(Merge Sort) problem in Parallel and Distributed Systems course,
 * University of Pisa, Italy)
 * You are free to use, modify, and distribute this code for educational purposes.
 */
#include "main.hpp"

int main(int argc, char **argv)
{
    /**
     * we will first estimate the size of the input data stream. If the size is within the MEMORY_CAP, we will proceed with in-memory sorting using std::sort. Otherwise, we will proceed with external sorting.
     */
    parse_cli_and_set(argc, argv);
    TimerClass seq_sort_time;
    spdlog::info("==> Calculating INPUT_SIZE <==");
    uint64_t stream_size = estimate_stream_size();
    /**
     * if the estimated stream size is less than or equal to the memory limit, we will sort in memory. Otherwise, we will perform out-of-core sorting.
     */
    try
    {
        {
            TimerScope sst(seq_sort_time);
            if (stream_size <= MEMORY_CAP)
            {
                spdlog::info("==> Starting IN_MEMORY_OPERATION: {}GiB <==", MEMORY_CAP / IN_GB);
                sort_in_memory();
            }
            else
            {
                spdlog::info("==> Starting OUT_OF_MEMORY_BOUND_OPERATION: {}GiB and DATA:{}GiB <==", MEMORY_CAP / IN_GB, stream_size / IN_GB);
                sort_out_of_core();
            }
        }
        spdlog::info("->[Timer] : Total Sequential Sorting Time -> {}", seq_sort_time.result());
        spdlog::info("==> Completed: Merge Sort, output -> {} <==", DATA_OUTPUT);
    }
    catch (const std::exception &error)
    {
        spdlog::error("==> X Operation aborted due to: {} X <==", error.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}