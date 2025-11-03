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

int main(int argc, char *argv[])
{
    /**
     * ------------- Setting Parameters -------------
     */
    spdlog::info("==> Setting Parameters Based on Inputs <==");
    if (argc != 7)
    {
        spdlog::error("Parameters should be == 7 | ./ff_farm -d X -m X -w X");
        throw std::runtime_error("");
    }
    else
    {
        std::string working_data_size = argv[3];
        std::string working_memory_size = argv[5];
        uint64_t demanded_workers = std::stoul(argv[7]);
        common::set_working_parameters(working_data_size, working_memory_size, demanded_workers);
    }

    spdlog::info("==> Welcome to the FARM experiment of our MERGE_SORT <==");
    spdlog::info("==> Emitter(CreateChunks) -> Worker(N_Sorters) -> Collector(Sink) -> sorted_records.bin <==");
    const uint64_t STREAM_SIZE = common::estimate_stream_size();
    spdlog::info("Number of Workers assigned: {}", WORKERS);
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
            spdlog::info("Creating CHUNKS based on MEMORY_CAP {} GiB", MEMORY_CAP / (1024ULL * 1024ULL * 1024ULL));
            auto report = common::get_summary(STREAM_SIZE, MEMORY_CAP, WORKERS);
            spdlog::info("Summary: {}", report.summary);
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