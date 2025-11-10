#pragma once
#include "data_structure.hpp"
#include <iostream>
#include <cstdint>
#include <vector>

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
    // generates one final output
    void merge_runs_to_file(const std::vector<std::string> &temp_paths,
                            const std::string &out_path);
}