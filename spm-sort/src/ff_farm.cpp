#include "common.hpp"
#include "ff/ff.hpp"
#include "constants.hpp"
#include "helper_ff.hpp"
#include "spdlog/spdlog.h"
#include "data_structure.hpp"
#include "../../utils/microtimer.h"
/*----------------------------------*/
#include <iostream>
#include <cstdint>

int main()
{
    spdlog::info("==> Welcome to the FARM experiment of our MERGE_SORT.....");
    spdlog::info("==> Emitter(CreateChunks) -> Worker(N_Sorters) -> Collector(Sink) -> sorted_records.bin");
    const uint64_t STREAM_SIZE = common::estimate_stream_size();
    const int WORKERS = ff_common::detect_workers();
    spdlog::info("==> Number of Workers assigned: {}", WORKERS);
    try
    {
        if (STREAM_SIZE < MEMORY_CAP)
        {
            spdlog::info("Mode: In-Memory FARM");
            ff_farm_in_memory::EMITTER emitter(DATA_IN_STREAM, WORKERS);
            ff_farm_in_memory::COLLECTOR collector(DATA_OUT_STREAM);

            ff::ff_Farm<ff_common::ITEMS, ff_common::ITEMS> farm(
                [&]()
                {
                    std::vector<std::unique_ptr<ff::ff_node>> workers;
                    workers.reserve(WORKERS);
                    for (int i = 0; i < WORKERS; ++i)
                        workers.push_back(std::make_unique<ff_farm_in_memory::WORKER>());
                    return workers;
                }());
            farm.add_emitter(emitter);
            farm.add_collector(collector);

            farm.set_scheduling_ondemand();
            // farm.set_scheduling_ondemand(4); // e.g., larger request queue

            if (farm.run_and_wait_end() < 0)
            {
                spdlog::error("Farm(inmem) failed");
                return EXIT_FAILURE;
            }
            spdlog::info("==> Completed in-memory FARM → {}", DATA_OUT_STREAM);
        }
        else
        {
            spdlog::info("Mode: Out-of-memory-core FARM");
            ff_farm_out_of_core::SegmentAndEmit emitter(DATA_IN_STREAM);
            ff_farm_out_of_core::CollectAndMerge collector(DATA_OUT_STREAM);

            ff::ff_Farm<ff_common::ITEMS, ff_common::ITEMS> farm(
                [&]()
                {
                    std::vector<std::unique_ptr<ff::ff_node>> workers;
                    workers.reserve(WORKERS);
                    for (int i = 0; i < WORKERS; ++i)
                        workers.push_back(std::make_unique<ff_farm_out_of_core::SortAndWriteSegment>());
                    return workers;
                }());
            farm.add_emitter(emitter);
            farm.add_collector(collector);

            farm.set_scheduling_ondemand();
            // farm.set_scheduling_ondemand(4); // e.g., larger request queue

            if (farm.run_and_wait_end() < 0)
            {
                spdlog::error("Farm(inmem) failed");
                return EXIT_FAILURE;
            }
            spdlog::info("==> Completed in-memory FARM → {}", DATA_OUT_STREAM);
        }
    }
    catch (const std::exception &error)
    {
        spdlog::error("==> X -> Operation aborted due to: {}", error.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}