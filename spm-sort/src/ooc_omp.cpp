#include <iostream>
#include <cstdint>
#include <fstream>
#include "constants.hpp"
#include "helper.hpp"
#include "common.hpp"
#include "../../utils/microtimer.h"

int main()
{

    spdlog::info("==> PHASE: 1 -> Calculate the stream size.....");
    uint64_t stream_size = common::estimate_stream_size();
    spdlog::info("==> PHASE: 2 -> Check memory cap and decide sort in/out memory bound.....");
    try
    {
        if (stream_size <= MEMORY_CAP)
        {
            spdlog::info("==> PHASE: 2.1 -> Starting in-memory operation.....");
            omp_sort::sort_in_memory_omp();
            spdlog::info("==> Completed: Merge Sort, data written in path: {}", DATA_OUT_STREAM);
            return EXIT_SUCCESS;
        }

        // Out-of-core: same chunking as Phase 4, but sort each run in parallel + grouped parallel merge

        spdlog::info("==> PHASE: 2.1 -> Starting out-of-memory-bound operation.....");
        omp_sort::sort_out_of_core_omp();
        spdlog::info("==> Completed: Merge Sort, data written in path: {}", DATA_OUT_STREAM);
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        spdlog::error("==> X -> Operation aborted due to: {}", error.what());
        return EXIT_FAILURE;
    }
}