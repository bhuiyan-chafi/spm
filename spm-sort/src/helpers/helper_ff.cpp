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

namespace ff_pipe_in_memory
{
    using ITEMS = ff_common::ITEMS;
    // 'in' is stream is defined within definition and is private for this
    DataLoader::DataLoader(const std::string &inpath) : in(inpath, std::ios::binary)
    {
        if (!in)
        {
            spdlog::error("==> X => Error in in/out DATA_STREAM.....");
            throw std::runtime_error("");
        }
    }

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