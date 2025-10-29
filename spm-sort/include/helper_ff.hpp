#pragma once
#include "data_structure.hpp"
#include "ff/ff.hpp"
#include <iostream>
#include <cstdint>
#include <vector>

namespace ff_common
{
    using ITEMS = std::vector<Item>;
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