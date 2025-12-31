/**
 * @author ASM CHAFIULLAH BHUIYAN
 * @file ff_pipe.cpp
 * @test just to see why pipeline as a solution is bad/good for our problem
 * @details we are trying a pure PIPELINE for our MERGE_SORT problem to check why it fit or doesn't
 * fit as an optimal solution.
 */
/**
 * ==> User defined HEADERS
 */
#include "helper_ff.hpp"
#include "common.hpp"
#include "constants.hpp"
#include "spdlog/spdlog.h"
#include "../../utils/microtimer.h"

/**
 * ==> Standard HEADERS
 */
#include <iostream>
#include <cstdint>
/**
 * ==> User defined constants
 * @param WORKERS to define how many worker we want for the pipeline, typical choice is to have
 * WORKERS = Number of Stages
 */
const int WORKERS = 0;
int main()
{
    spdlog::info("==> Welcome to the Pipeline experiment of our MERGE_SORT problem with 4 stages");
    spdlog::info("==> Segmenter -> ChunkSorter -> ChunkWriter -> ChunkMerger -> sorted_records.bin");
    spdlog::info("==> PHASE: 1 -> Calculate the stream size.....");
    const uint64_t STREAM_SIZE = common::estimate_stream_size();
    spdlog::info("==> PHASE: 2 -> Check memory cap and decide sort in/out memory bound.....");
    try
    {
        if (STREAM_SIZE < MEMORY_CAP)
        {
            spdlog::info("==> PHASE: 2.1 -> Starting in-memory operation.....");
            spdlog::info("==> PHASE: 3 -> STAGE_1: Starting up DATA_LOADER stage.....");
            ff_pipe_in_memory::DataLoader loader(DATA_IN_STREAM);
            spdlog::info("==> PHASE: 4 -> STAGE_2: Starting up DATA_SORTER stage.....");
            ff_pipe_in_memory::DataSorter sorter;
            spdlog::info("==> PHASE: 5 -> STAGE_3: Starting up DATA_WRITER stage.....");
            ff_pipe_in_memory::DataWriter writer(DATA_OUT_STREAM);
            spdlog::info("==> PHASE: 6 -> Starting the PIPELINE parallelization.....");
            ff::ff_Pipe pipe(loader, sorter, writer);
            if (pipe.run_and_wait_end() < 0)
            {
                spdlog::error("==> X -> Pipeline failed .......");
                return EXIT_FAILURE;
            }
            spdlog::info("==> Completed: Merge Sort, data written in path: {}", DATA_OUT_STREAM);
        }
        else
        {
            spdlog::info("==> PHASE: 2.1 -> Starting out-of-memory-bound operation.....");
            // spdlog::info("==> PHASE: 3 -> STAGE_1: Creating CHUNK_LOADER stage/node.....");
            ff_pipe_out_of_core::Segmenter segmenter(DATA_IN_STREAM);
            // spdlog::info("==> PHASE: 4 -> STAGE_2: Creating DATA_SORTER stage/node.....");
            ff_pipe_out_of_core::ChunkSorter sorter;
            // spdlog::info("==> PHASE: 5 -> STAGE_3: Creating CHUNK_WRITER stage/node.....");
            ff_pipe_out_of_core::ChunkWriter writer;
            // spdlog::info("==> PHASE: 6 -> STAGE_4: Creating CHUNK_MERGER stage/node.....");
            ff_pipe_out_of_core::ChunkMerger merger(DATA_OUT_STREAM);
            spdlog::info("==> PHASE: 3 -> Starting the PIPELINE parallelization.....");
            ff::ff_Pipe pipe(segmenter, sorter, writer, merger);
            if (pipe.run_and_wait_end() < 0)
            {
                spdlog::error("==> X -> Pipeline failed .......");
                return EXIT_FAILURE;
            }
            spdlog::info("==> Completed: Merge Sort, data written in path: {}", DATA_OUT_STREAM);
        }
    }
    catch (const std::exception &error)
    {
        spdlog::error("==> X -> Operation aborted due to: {}", error.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
