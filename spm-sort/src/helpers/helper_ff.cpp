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

namespace ff_pipe_in_memory
{
    using ITEMS = ff_common::ITEMS;
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
    int DataLoader::svc_init()
    {
        spdlog::info("[init] ReaderAll tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        return 0;
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

    int DataSorter::svc_init()
    {
        spdlog::info("[init] ReaderAll tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        return 0;
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

    int DataWriter::svc_init()
    {
        spdlog::info("[init] ReaderAll tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        return 0;
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
    ChunkLoader::ChunkLoader(const std::string &inpath) : in(inpath, std::ios::binary)
    {
        if (!in)
        {
            spdlog::error("==> X => Error in in/out DATA_STREAM.....");
            throw std::runtime_error("");
        }
    }
    int ChunkLoader::svc_init()
    {
        spdlog::info("[init] ReaderAll tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        return 0;
    }
    TEMP_ITEMS *ChunkLoader::svc(TEMP_ITEMS *)
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
                    spdlog::info("No more items in DATA_STREAM, Releasing the last CHUNK_{}", current_stream_index);
                    ff_send_out(temp_items.release());
                    // freeing memory
                    temp_items = std::make_unique<TEMP_ITEMS>();
                    current_stream_size_inBytes = 0ULL;
                }
                break;
            }
            current_item_size = sizeof(uint64_t) + sizeof(uint32_t) + payload.size() + sizeof(uint16_t);
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
    int ChunkSorter::svc_init()
    {
        spdlog::info("[init] ReaderAll tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        return 0;
    }
    TEMP_ITEMS *ChunkSorter::svc(TEMP_ITEMS *temp_items)
    {
        spdlog::info("Processing Thread for CHUNK_SORTER tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        spdlog::info("Sorting CHUNK of size: {} ", temp_items->size());
        std::sort(temp_items->begin(), temp_items->end(), [](const Item &a, const Item &b)
                  { return a.key < b.key; });
        return temp_items;
    }

    /** ----- Write the released CHUNKS ----- */
    int ChunkWriter::svc_init()
    {
        spdlog::info("[init] ReaderAll tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        return 0;
    }
    STRING_VECTOR *ChunkWriter::svc(TEMP_ITEMS *temp_items)
    {
        spdlog::info("Processing Thread for CHUNK_WRITER tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        spdlog::info("Writing CHUNK_{} of size: {} ", current_stream_index, temp_items->size());
        temp_chunk_out_path = DATA_TMP_DIR + "run_" + std::to_string(current_stream_index++) + ".bin";
        common::write_temp_chunk(temp_chunk_out_path, *temp_items);
        delete temp_items;
        temp_chunk_out_paths.push_back(temp_chunk_out_path);
        // accumulate everything at svc_env()
        return GO_ON;
    }

    void ChunkWriter::svc_end()
    {
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
    int ChunkMerger::svc_init()
    {
        spdlog::info("[init] ReaderAll tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
        return 0;
    }
    void *ChunkMerger::svc(STRING_VECTOR *temp_record_paths)
    {
        spdlog::info("Processing Thread for CHUNK_MERGER tid={} cpu={}", ff_common::get_tid(), sched_getcpu());
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
        spdlog::info("==> PHASE: 10 -> Cleaning up data chunks.....");
        for (const auto &temp_record_path : *temp_record_paths)
        {
            std::remove(temp_record_path.c_str());
        }
        delete temp_record_paths;
        return GO_ON;
    }
}