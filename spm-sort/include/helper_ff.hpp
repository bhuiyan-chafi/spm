#pragma once
#include "data_structure.hpp"
#include "ff/ff.hpp"
#include <iostream>
#include <cstdint>
#include <vector>

// ---------- Thread instrumentation helpers ----------
#include <sys/syscall.h>
#include <unistd.h>
#include <sched.h>

namespace ff_common
{
    using ITEMS = std::vector<Item>;
    static inline unsigned long get_tid()
    {
        return static_cast<unsigned long>(::syscall(SYS_gettid));
    }
}

namespace ff_pipe_in_memory
{
    using ITEMS = ff_common::ITEMS;
    struct DataLoader : ff::ff_node_t<ITEMS>
    {
        int svc_init() override;
        explicit DataLoader(const std::string &inpath);
        ITEMS *svc(ITEMS *) override;

    private:
        std::ifstream in;
    };
    struct DataSorter : ff::ff_node_t<ITEMS, ITEMS>
    {
        int svc_init() override;
        ITEMS *svc(ITEMS *items) override;
    };
    struct DataWriter : ff::ff_node_t<ITEMS, void>
    {
        int svc_init() override;
        explicit DataWriter(const std::string &outpath);
        void *svc(ITEMS *items) override;

    private:
        std::ofstream out;
    };
}

namespace ff_pipe_out_of_core
{
    using TEMP_ITEMS = ff_common::ITEMS;
    using STRING_VECTOR = std::vector<std::string>;
    /** ----- Create CHUNKS and RELEASE to get SORTED ----- */
    struct ChunkLoader : ff::ff_node_t<TEMP_ITEMS>
    {
        int svc_init() override;
        explicit ChunkLoader(const std::string &inpath);
        TEMP_ITEMS *svc(TEMP_ITEMS *) override;

    private:
        std::ifstream in;
        /** ----- Each THEAD will handle one STAGE so there will be no RACE_CONDITION here */
        uint64_t current_stream_size_inBytes = 0ULL;
        uint64_t current_stream_index = 0;
        uint64_t current_item_size = 0ULL;
    };
    /** ----- Sort the released CHUNKS ----- */
    struct ChunkSorter : ff::ff_node_t<TEMP_ITEMS, TEMP_ITEMS>
    {
        int svc_init() override;
        TEMP_ITEMS *svc(TEMP_ITEMS *) override;
    };

    /** ----- Write the released CHUNKS ----- */
    struct ChunkWriter : ff::ff_node_t<TEMP_ITEMS, STRING_VECTOR>
    {
        int svc_init() override;
        STRING_VECTOR *svc(TEMP_ITEMS *) override;
        // at the end we accumulate all paths and push into one
        void svc_end() override;

    private:
        uint64_t current_stream_index = 0;
        std::string temp_chunk_out_path;
        STRING_VECTOR temp_chunk_out_paths;
    };

    /** ----- MERGE the released CHUNKS ----- */
    struct ChunkMerger : ff::ff_node_t<STRING_VECTOR, void>
    {
        int svc_init() override;
        explicit ChunkMerger(const std::string &outpath);
        void *svc(STRING_VECTOR *temp_record_paths) override;

    private:
        std::ofstream out;
        std::vector<std::unique_ptr<TempReader>> readers;
    };
}