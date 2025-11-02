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
#include <thread>

namespace ff_common
{
    using ITEMS = std::vector<Item>;
    static inline unsigned long get_tid()
    {
        return static_cast<unsigned long>(::syscall(SYS_gettid));
    }

    static inline int detect_workers()
    {

        uint16_t WORKERS{0};
        return WORKERS = std::stoi(std::getenv("SLURM_CPUS_PER_TASK") ?: std::to_string(ff_numCores()));
    }

    inline static std::vector<std::pair<size_t, size_t>> create_chunk_ranges(size_t n, size_t parts)
    {
        std::vector<std::pair<size_t, size_t>> ranges;
        if (parts == 0)
            return ranges;
        ranges.reserve(parts);
        for (size_t i = 0; i < parts; ++i)
        {
            size_t L = (i * n) / parts;
            size_t R = ((i + 1) * n) / parts;
            ranges.emplace_back(L, R);
        }
        return ranges;
    }
}

namespace ff_pipe_in_memory
{
    using ITEMS = ff_common::ITEMS;
    struct DataLoader : ff::ff_node_t<ITEMS>
    {

        explicit DataLoader(const std::string &inpath);
        ITEMS *svc(ITEMS *) override;

    private:
        std::ifstream in;
    };
    struct DataSorter : ff::ff_node_t<ITEMS, ITEMS>
    {
        ITEMS *svc(ITEMS *items) override;
    };
    struct DataWriter : ff::ff_node_t<ITEMS, void>
    {
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
    struct Segmenter : ff::ff_node_t<TEMP_ITEMS>
    {

        explicit Segmenter(const std::string &inpath);
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
        TEMP_ITEMS *svc(TEMP_ITEMS *) override;
    };

    /** ----- Write the released CHUNKS ----- */
    struct ChunkWriter : ff::ff_node_t<TEMP_ITEMS, STRING_VECTOR>
    {
        STRING_VECTOR *svc(TEMP_ITEMS *) override;
        // at the end we accumulate all paths and push into one
        void eosnotify(ssize_t) override;

    private:
        uint64_t current_stream_index = 0;
        std::string temp_chunk_out_path;
        STRING_VECTOR temp_chunk_out_paths;
    };

    /** ----- MERGE the released CHUNKS ----- */
    struct ChunkMerger : ff::ff_node_t<STRING_VECTOR, void>
    {
        explicit ChunkMerger(const std::string &outpath);
        void *svc(STRING_VECTOR *temp_record_paths) override;

    private:
        std::ofstream out;
        std::vector<std::unique_ptr<TempReader>> readers;
    };
}

namespace ff_farm_in_memory
{
    using ITEMS = ff_common::ITEMS;
    struct EMITTER : ff::ff_node_t<ITEMS>
    {
        explicit EMITTER(const std::string &inpath, int parts);
        ITEMS *svc(ITEMS *) override;

    private:
        std::ifstream in;
        uint16_t parts;
    };

    struct WORKER : ff::ff_node_t<ITEMS, ITEMS>
    {
        ITEMS *svc(ITEMS *items) override;
    };

    struct COLLECTOR : ff::ff_node_t<ITEMS, void>
    {
        explicit COLLECTOR(const std::string &outpath);
        void *svc(ITEMS *v) override;
        void svc_end() override;

    private:
    private:
        std::ofstream out;
        std::vector<ITEMS *> batches;
    };
}

namespace ff_farm_out_of_core
{
    using TEMP_ITEMS = ff_common::ITEMS;
    using STRING_VECTOR = std::vector<std::string>;
    using TEMP_SEG_PATH = std::string;
    /** ----- Create SEGMENTS and CHUNKS and RELEASE to get SORTED ----- */
    struct SegmentAndEmit : ff::ff_node_t<TEMP_ITEMS>
    {

        explicit SegmentAndEmit(const std::string &inpath);
        TEMP_ITEMS *svc(TEMP_ITEMS *) override;

    private:
        std::ifstream in;
        /**
         * ----- Each segment will have different chunks fed to different workers -----
         * Are we going to have race conditions?
         *
         **/
        uint64_t current_stream_size_inBytes = 0ULL;
        uint64_t current_stream_index = 0;
        uint64_t current_item_size = 0ULL;
    };
    /** ----- Sort and Write the released Segments ----- */
    struct SortAndWriteSegment : ff::ff_node_t<TEMP_ITEMS, TEMP_SEG_PATH>
    {
        TEMP_SEG_PATH *svc(TEMP_ITEMS *) override;
    };

    /** ----- Collect all Segments and Merge them into one file ----- */
    struct CollectAndMerge : ff::ff_node_t<TEMP_SEG_PATH, void>
    {
        explicit CollectAndMerge(const std::string &outpath);
        void *svc(TEMP_SEG_PATH *path) override;
        void svc_end() override;

    private:
        std::ofstream out;
        STRING_VECTOR segment_paths;
        std::vector<std::unique_ptr<TempReader>> readers;
    };
}