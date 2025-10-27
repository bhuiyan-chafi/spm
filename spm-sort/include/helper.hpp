#pragma once
#include <iostream>
#include <cstdint>
#include "data_structure.hpp"

namespace common
{
    // calculate stream size to decide where(in/out bound of memory) to process the data
    uint64_t estimate_stream_size();
    // writing chunks inside ../data/temp/
    void write_temp_chunk(std::string &temp_chunk_out_path, const std::vector<Item> &temp_item_chunk);
}

namespace seq_sort
{
    // sort loading everything in memory
    void sort_in_memory();
    // sort in chunks, out of memory capacity(MEMORY_CAP)
    void sort_out_of_core();
}

namespace omp_sort
{
    // omp operation if data fit in memory
    void sort_in_memory_omp();
    // omp operation if data doesn't fit in memory
    void sort_out_of_core_omp();
    // each chunk is ran in parallel by different threads
    void sort_run_parallel(std::vector<Item> &input);
    // merge the temp files from each thread
    void parallel_grouped_merge(const std::vector<std::string> &run_paths,
                                const std::string &final_out,
                                const std::string &tmp_dir);
    // generate one final output
    void merge_runs_to_file(const std::vector<std::string> &temp_paths,
                            const std::string &out_path);
}
