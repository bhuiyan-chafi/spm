#include "helper_ff.hpp"
#include "common.hpp"
#include "data_structure.hpp"
#include "record.hpp"
#include "constants.hpp"
#include "spdlog/spdlog.h"

#include <iostream>
#include <fstream>
#include <queue>
#include <filesystem>
#include <algorithm>
#include <atomic>
#include <malloc.h>

namespace ff_pipe_in_memory
{
    // 'in' is stream is defined within definition and is private for this
    // NODE::FUNCTION from the namespace
    DataLoader::DataLoader(const std::string &inpath) : in(inpath, std::ios::binary)
    {
        if (!in)
        {
            spdlog::error("==> X => Error in in/out DATA_STREAM.....");
            throw std::runtime_error("");
        }
    }

    // return_type *Node::function(parameter)
    ITEMS *DataLoader::svc(ITEMS *)
    {
        auto items = std::make_unique<ITEMS>();
        common::load_all_data_in_memory(*items, in);
        spdlog::info("==> PHASE: 7 -> DATA_LOADER completed loading data in memory.....");
        spdlog::info("TOTAL DATA: {}", items->size());
        ff_send_out(items.release());
        // End_Of_Service, that's why the OUT_t is absent in the definition
        return EOS;
    }

    ITEMS *DataSorter::svc(ITEMS *items)
    {
        std::sort(items->begin(), items->end(), [](const Item &a, const Item &b)
                  { return a.key < b.key; });
        spdlog::info("==> PHASE: 8 -> DATA_SORTER completed sorting data in memory.....");
        return items;
    }

    DataWriter::DataWriter(const std::string &outpath) : out(outpath, std::ios::binary)
    {
        if (!out)
        {
            spdlog::error("==> X => Error in out DATA_STREAM.....");
            throw std::runtime_error("");
        }
    }

    void *DataWriter::svc(ITEMS *items)
    {
        for (auto &it : *items)
            recordHeader::write_record(out, it.key, it.payload);
        spdlog::info("==> PHASE: 9 -> DATA_WRITER completed writing data in memory.....");
        delete items;
        spdlog::info("==> PHASE: 10 -> DATA_WRITER completed clearing memory.....");
        return GO_ON;
    }
}

namespace ff_pipe_out_of_core
{
    using TEMP_ITEMS = ff_common::ITEMS;
    // 'in' is stream is defined within definition and is private for this
    // NODE::FUNCTION from the namespace
    /** ----- Create CHUNKS and RELEASE to get SORTED ----- */
    Segmenter::Segmenter(const std::string &inpath) : in(inpath, std::ios::binary)
    {
        if (!in)
        {
            spdlog::error("==> X => Error in in/out DATA_STREAM.....");
            throw std::runtime_error("");
        }
    }

    TEMP_ITEMS *Segmenter::svc(TEMP_ITEMS *)
    {
        // spdlog::info("Processing Thread for CHUNK_LOADER tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        auto temp_items = std::make_unique<TEMP_ITEMS>();
        while (true)
        {
            uint64_t key;
            std::vector<uint8_t> payload;
            if (!recordHeader::read_record(in, key, payload))
            {
                if (!temp_items->empty())
                {
                    // spdlog::info("No more items in DATA_STREAM, Releasing the last CHUNK_{}", current_stream_index);
                    ff_send_out(temp_items.release());
                    // freeing memory
                    temp_items = std::make_unique<TEMP_ITEMS>();
                    current_stream_size_inBytes = 0ULL;
                }
                break;
            }
            current_item_size = sizeof(uint64_t) + sizeof(uint32_t) + payload.size();
            if ((current_stream_size_inBytes + current_item_size) > MEMORY_CAP)
            {
                spdlog::info("Releasing CHUNK_{}", current_stream_index++);
                ff_send_out(temp_items.release());
                // freeing memory
                temp_items = std::make_unique<TEMP_ITEMS>();
                current_stream_size_inBytes = 0ULL;
            }
            temp_items->push_back(Item{key, std::move(payload)});
            current_stream_size_inBytes += current_item_size;
        }
        return EOS;
    }

    /** ----- Sort the released CHUNKS ----- */

    TEMP_ITEMS *ChunkSorter::svc(TEMP_ITEMS *temp_items)
    {
        // spdlog::info("Processing Thread for CHUNK_SORTER tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        // spdlog::info("Sorting CHUNK of size: {} ", temp_items->size());
        std::sort(temp_items->begin(), temp_items->end(), [](const Item &a, const Item &b)
                  { return a.key < b.key; });
        return temp_items;
    }

    /** ----- Write the released CHUNKS ----- */

    STRING_VECTOR *ChunkWriter::svc(TEMP_ITEMS *temp_items)
    {
        // spdlog::info("Processing Thread for CHUNK_WRITER tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        spdlog::info("Writing CHUNK_{} of size: {} ", current_stream_index, temp_items->size());
        temp_chunk_out_path = DATA_TMP_DIR + "run_" + std::to_string(current_stream_index++) + ".bin";
        common::write_temp_chunk(temp_chunk_out_path, *temp_items);
        delete temp_items;
        temp_chunk_out_paths.push_back(temp_chunk_out_path);
        // accumulate everything at svc_env()
        return GO_ON;
    }

    void ChunkWriter::eosnotify(ssize_t)
    {
        if (temp_chunk_out_paths.empty())
        {
            spdlog::error("No runs produced; nothing to merge.");
            return;
        }
        // spdlog::info("Emitting {} chunk paths", temp_chunk_out_paths.size());
        auto *paths = new STRING_VECTOR;
        paths->swap(temp_chunk_out_paths);
        ff_send_out(paths);
    }

    /** ----- MERGE the released CHUNKS ----- */
    ChunkMerger::ChunkMerger(const std::string &outpath) : out(outpath, std::ios::binary)
    {
        if (!out)
        {
            spdlog::error("==> X => Error in out DATA_STREAM.....");
            throw std::runtime_error("");
        }
    }
    /**
     * ----- currently performing two tasks : loading in HEAP and WRITE -----
     * We will see in future if we can separate them
     */

    void *ChunkMerger::svc(STRING_VECTOR *temp_record_paths)
    {
        spdlog::info("Merging {} temporary runs:", temp_record_paths->size());
        /**
         * for (size_t i = 0; i < temp_record_paths->size(); ++i)
         * {
         *  spdlog::info("  [{}] {}", i, (*temp_record_paths)[i]);
         * }
         */

        readers.reserve(temp_record_paths->size());
        for (const auto &record_path : *temp_record_paths)
            readers.push_back(std::make_unique<TempReader>(record_path));

        std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> heap;
        for (uint64_t i = 0; i < readers.size(); ++i)
        {
            if (!readers[i]->eof)
                heap.push(HeapNode{readers[i]->key, i});
        }
        spdlog::info("Writing RECORDS from data chunks.....");
        while (!heap.empty())
        {
            auto top = heap.top();
            heap.pop();
            auto &current_reader = *readers[top.temp_run_index];
            recordHeader::write_record(out, current_reader.key, current_reader.payload);
            current_reader.advance();
            if (!current_reader.eof)
            {
                heap.push(HeapNode{current_reader.key, top.temp_run_index});
            }
        }
        spdlog::info("Cleaning up data chunks.....");
        for (const auto &temp_record_path : *temp_record_paths)
        {
            std::remove(temp_record_path.c_str());
        }
        delete temp_record_paths;
        return GO_ON;
    }
}

namespace ff_farm_in_memory
{
    /**
     * ----- EMITTER(chunk creator) Functionalities -----
     * @param parts = WORKERS
     * */
    EMITTER::EMITTER(const std::string &inpath, int parts)
        : in(inpath, std::ios::binary), parts(parts)
    {
        if (!in)
        {
            spdlog::error("==> X => Error in in/out DATA_STREAM.....");
            throw std::runtime_error("");
        }
    }
    ITEMS *EMITTER::svc(ITEMS *)
    {
        // since FF works through pointers
        spdlog::info("[init] EMITTER_Worker tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        auto all = std::make_unique<ITEMS>();
        common::load_all_data_in_memory(*all, in);
        spdlog::info("Emitter: loaded {} records", all->size());
        /**
         * we will create chunks based on number of workers
         * we did the same thing in OPENMP
         * size/THREADS -> vector<ITEM> chunks
         *
         * Here it's similar, we are taking the whole size of input and number of WORKERs
         * then we decide the ranges to be sorted by each workers
         */
        auto ranges = ff_common::create_chunk_ranges(all->size(), parts);
        // how many items we have
        for (size_t i = 0; i < ranges.size(); ++i)
        {
            auto [START, END] = ranges[i];
            auto *chunk = new ITEMS;
            chunk->reserve(END - START);
            // for each item's START and END
            for (size_t j = START; j < END; ++j)
                chunk->push_back(std::move((*all)[j]));
            spdlog::info("Emitter: emits chunk_{} size={}", i, chunk->size());
            // keeps sending until the chunks are finished
            ff_send_out(chunk);
        }
        // end of service
        return this->EOS;
    }

    /**
     * ----- WORKER(sorter) Functionalities -----
     * */
    ITEMS *WORKER::svc(ITEMS *items)
    {
        spdlog::info("[init] WORKER_Sorter tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        std::sort(items->begin(), items->end(),
                  [](const Item &a, const Item &b)
                  { return a.key < b.key; });
        return items;
    }

    /**
     * ----- COLLECTOR(sink) Functionalities -----
     */
    COLLECTOR::COLLECTOR(const std::string &outpath) : out(outpath, std::ios::binary)
    {
        spdlog::info("[init] COLLECTOR tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        if (!out)
        {
            spdlog::error("==> X => Error in in/out DATA_STREAM.....");
            throw std::runtime_error("");
        }
    }
    void *COLLECTOR::svc(ITEMS *v)
    {
        batches.emplace_back(v); // take ownership
        return this->GO_ON;
    }
    void COLLECTOR::svc_end()
    {
        spdlog::info("COLLECTOR: merging {} sorted batches", batches.size());
        auto comparison_key = [](const Node &a, const Node &b)
        { return a.key > b.key; };
        std::priority_queue<Node, std::vector<Node>, decltype(comparison_key)> comparison_queue(comparison_key);
        for (uint64_t index = 0; index < batches.size(); ++index)
            if (!batches[index]->empty())
                comparison_queue.push(Node{(*batches[index])[0].key, index, 0});
        size_t written = 0;
        /**
         * Writing records from each worker's heap
         */
        while (!comparison_queue.empty())
        {
            auto node = comparison_queue.top();
            comparison_queue.pop();
            const Item &it = (*batches[node.sub_range])[node.offset];
            recordHeader::write_record(out, it.key, it.payload);
            ++written;
            std::uint64_t next_i = node.offset + 1;
            if (next_i < batches[node.sub_range]->size())
            {
                comparison_queue.push(Node{
                    (*batches[node.sub_range])[next_i].key,
                    node.sub_range,
                    next_i,
                });
            }
        }
        spdlog::info("COLLECTOR: wrote {} records -> {}", written, DATA_OUT_STREAM);
        /**
         * Clearing items
         */
        for (auto *p : batches)
            delete p;
        batches.clear();
        spdlog::info("COLLECTOR: cleared records from MEMORY");
    }
}

namespace ff_farm_out_of_core
{
    /** ----- Create CHUNKS and RELEASE to get SORTED ----- */
    SegmentAndEmit::SegmentAndEmit(const std::string &inpath) : in(inpath, std::ios::binary)
    {
        if (!in)
        {
            spdlog::error("==> X => Error in in/out DATA_STREAM.....");
            throw std::runtime_error("");
        }
    }
    TEMP_ITEMS *SegmentAndEmit::svc(TEMP_ITEMS *)
    {
        spdlog::info("Processing Thread for CHUNK_LOADER tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        spdlog::info("Creating CHUNKS based on MEMORY_CAP {} GiB", MEMORY_CAP / (1024ULL * 1024ULL * 1024ULL));
        auto temp_items = std::make_unique<TEMP_ITEMS>();
        while (true)
        {
            uint64_t key;
            std::vector<uint8_t> payload;
            if (!recordHeader::read_record(in, key, payload))
            {
                if (!temp_items->empty())
                {
                    // spdlog::info("No more items in DATA_STREAM, Releasing the last CHUNK_{}", current_stream_index);
                    ff_send_out(temp_items.release());
                    // freeing memory
                    temp_items = std::make_unique<TEMP_ITEMS>();
                    current_stream_size_inBytes = 0ULL;
                }
                break;
            }
            current_item_size = sizeof(uint64_t) + sizeof(uint32_t) + payload.size();
            if ((current_stream_size_inBytes + current_item_size) > MEMORY_CAP)
            {
                spdlog::info("Releasing CHUNK_{}", current_stream_index++);
                ff_send_out(temp_items.release());
                // freeing memory from emitters side because workers has the as private
                temp_items = std::make_unique<TEMP_ITEMS>();
                current_stream_size_inBytes = 0ULL;
            }
            temp_items->push_back(Item{key, std::move(payload)});
            current_stream_size_inBytes += current_item_size;
        }
        return EOS;
    }
    /** ----- Sort the released CHUNKS ----- */

    TEMP_SEG_PATH *SortAndWriteSegment::svc(TEMP_ITEMS *items)
    {
        spdlog::info("Processing Thread for CHUNK_LOADER tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        std::sort(items->begin(), items->end(),
                  [](const Item &a, const Item &b)
                  { return a.key < b.key; });

        /**
         * Each worker sorts and writes it's own segment
         * then they will return one path, which is unique
         * race-condition will happen
         * so we need atomic operation while writing the file name
         */
        static std::atomic<uint64_t> segment_index{0};
        const auto index = segment_index.fetch_add(1, std::memory_order_relaxed);
        const std::string current_segment_path = DATA_TMP_DIR + "run_" + std::to_string(index) + ".bin";
        spdlog::info("Current PATH: {}", current_segment_path);
        // reuse your helper to write a temp chunk
        auto segment_path = current_segment_path; // write_temp_chunk takes non-const ref
        common::write_temp_chunk(segment_path, *items);

        delete items;
        return new TEMP_SEG_PATH(current_segment_path);
    }

    /** ----- Collect and Merge ----- */
    CollectAndMerge::CollectAndMerge(const std::string &outpath) : out(outpath, std::ios::binary)
    {
        if (!out)
        {
            spdlog::error("==> X => Error in in/out DATA_STREAM.....");
            throw std::runtime_error("");
        }
    }
    // receives each path from the workers and fills in the path vector
    void *CollectAndMerge::svc(TEMP_SEG_PATH *path)
    {
        segment_paths.emplace_back(std::move(*path));
        delete path;
        return this->GO_ON;
    }
    // time to perform the final merge
    void CollectAndMerge::svc_end()
    {
        // to verify we are not loading data again memory for writing
        malloc_trim(0);
        spdlog::info("Merging {} temporary runs:", segment_paths.size());
        /**
         * for (size_t i = 0; i < temp_record_paths->size(); ++i)
         * {
         *  spdlog::info("  [{}] {}", i, (*temp_record_paths)[i]);
         * }
         */

        readers.reserve(segment_paths.size());
        for (const auto &record_path : segment_paths)
            readers.push_back(std::make_unique<TempReader>(record_path));

        std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> heap;
        for (uint64_t i = 0; i < readers.size(); ++i)
        {
            if (!readers[i]->eof)
                heap.push(HeapNode{readers[i]->key, i});
        }
        spdlog::info("Writing RECORDS from data chunks.....");
        while (!heap.empty())
        {
            auto top = heap.top();
            heap.pop();
            auto &current_reader = *readers[top.temp_run_index];
            recordHeader::write_record(out, current_reader.key, current_reader.payload);
            current_reader.advance();
            if (!current_reader.eof)
            {
                heap.push(HeapNode{current_reader.key, top.temp_run_index});
            }
        }
        spdlog::info("Cleaning up data chunks.....");
        for (const auto &temp_record_path : segment_paths)
        {
            std::remove(temp_record_path.c_str());
        }
        segment_paths.clear();
        readers.clear();
        readers.shrink_to_fit();
    }
}