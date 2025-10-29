/**
 * @file seq_sort.cpp
 * Author: ASM CHAFIULLAH BHUIYAN, M.Sc. in Computer Science and Network, University of Pisa, Italy
 * Contact: a.bhuiyan@studenti.unipi.it
 * Created on: October 2025
 * Project: Project-1(Merge Sort) problem in Parallel and Distributed Systems course,
 * University of Pisa, Italy)
 * You are free to use, modify, and distribute this code for educational purposes.
 */
#include <iostream>
#include <cstdint>
#include "common.hpp"
#include "helper.hpp"
#include "constants.hpp"
#include "spdlog/spdlog.h"
#include "../../utils/microtimer.h"

int main()
{
    /**
     * we will first estimate the size of the input data stream. If the size is within the MEMORY_CAP, we will proceed with in-memory sorting using std::sort. Otherwise, we will proceed with external sorting.
     */
    spdlog::info("==> PHASE: 1 -> Calculate the stream size.....");
    uint64_t stream_size = common::estimate_stream_size();
    /**
     * if the estimated stream size is less than or equal to the memory limit, we will sort in memory. Otherwise, we will perform out-of-core sorting.
     */
    spdlog::info("==> PHASE: 2 -> Check memory cap and decide sort in/out memory bound.....");
    try
    {
        if (stream_size <= MEMORY_CAP)
        {
            spdlog::info("==> PHASE: 2.1 -> Starting in-memory operation.....");
            seq_sort::sort_in_memory();
            spdlog::info("==> Completed: Merge Sort, data written in path: {}", DATA_OUT_STREAM);
            return EXIT_SUCCESS;
        }

        spdlog::info("==> PHASE: 2.1 -> Starting out-of-memory-bound operation.....");
        seq_sort::sort_out_of_core();
        spdlog::info("==> Completed: Merge Sort, data written in path: {}", DATA_OUT_STREAM);
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        spdlog::error("==> X -> Operation aborted due to: {}", error.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}