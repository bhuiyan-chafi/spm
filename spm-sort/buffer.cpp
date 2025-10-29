// Pure FastFlow pipelines (no farms) for both branches.
// - In-memory  (< MEM_CAP): ReaderAll -> Sorter -> WriterAll
// - Out-of-core(>= MEM_CAP): ReaderChunks -> Sorter -> Spiller -> Collector(eos->final merge)

#include <ff/pipeline.hpp>
#include <ff/node.hpp>
#include <filesystem>
#include <fstream>
#include <queue>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <atomic>

#include "spdlog/spdlog.h"
#include "record.hpp"
#include "data_structure.hpp" // Item{key,payload}

#ifndef DEFAULT_MEMORY_CAP_MIB
#define DEFAULT_MEMORY_CAP_MIB 2048ULL
#endif
#ifndef DEFAULT_CHUNK_MIB
#define DEFAULT_CHUNK_MIB 16ULL
#endif

using ItemVec = std::vector<Item>;
struct RunInfo
{
    std::string path;
    std::size_t items{};
};

// ---------- small io helpers
static inline std::uint64_t per_item_bytes(std::uint64_t, const std::vector<std::uint8_t> &p)
{
    return sizeof(std::uint64_t) + sizeof(std::uint32_t) + p.size();
}

// ---------- in-memory pipeline stages
struct ReaderAll : ff::ff_node_t<ItemVec>
{
    std::ifstream in;
    explicit ReaderAll(const std::string &inpath) : in(inpath, std::ios::binary)
    {
        if (!in)
            throw std::runtime_error("Cannot open input " + inpath);
    }
    ItemVec *svc(ItemVec *) override
    {
        auto data = std::make_unique<ItemVec>();
        std::uint64_t k;
        std::vector<std::uint8_t> p;
        while (recordHeader::read_record(in, k, p))
            data->push_back(Item{k, std::move(p)});
        spdlog::info("ReaderAll loaded {} records", data->size());
        ff_send_out(data.release());
        return EOS;
    }
};

struct SortStage_IV : ff::ff_node_t<ItemVec, ItemVec>
{
    ItemVec *svc(ItemVec *v) override
    {
        std::sort(v->begin(), v->end(), [](const Item &a, const Item &b)
                  { return a.key < b.key; });
        return v;
    }
};

struct WriterAll : ff::ff_node_t<ItemVec, void>
{
    std::string outpath;
    explicit WriterAll(std::string out) : outpath(std::move(out)) {}
    void *svc(ItemVec *v) override
    {
        std::ofstream out(outpath, std::ios::binary);
        if (!out)
        {
            delete v;
            throw std::runtime_error("Cannot open " + outpath);
        }
        for (auto &it : *v)
            recordHeader::write_record(out, it.key, it.payload);
        spdlog::info("WriterAll wrote {} records -> {}", v->size(), outpath);
        delete v;
        return GO_ON;
    }
};

// ---------- out-of-core pipeline stages
using Chunk = ItemVec;

struct ReaderChunks : ff::ff_node_t<Chunk>
{
    std::ifstream in;
    std::uint64_t chunk_bytes;
    ReaderChunks(const std::string &inpath, std::uint64_t chunkMiB)
        : in(inpath, std::ios::binary), chunk_bytes(chunkMiB * 1024ULL * 1024ULL)
    {
        if (!in)
            throw std::runtime_error("Cannot open input " + inpath);
    }
    Chunk *svc(Chunk *) override
    {
        while (true)
        {
            auto c = std::make_unique<Chunk>();
            std::uint64_t acc = 0;
            while (acc < chunk_bytes)
            {
                std::uint64_t k;
                std::vector<std::uint8_t> p;
                if (!recordHeader::read_record(in, k, p))
                    break;
                acc += per_item_bytes(k, p);
                c->push_back(Item{k, std::move(p)});
            }
            if (c->empty())
                break;
            ff_send_out(c.release());
        }
        return EOS;
    }
};

struct SortStage_OC : ff::ff_node_t<Chunk, Chunk>
{
    Chunk *svc(Chunk *c) override
    {
        std::sort(c->begin(), c->end(), [](const Item &a, const Item &b)
                  { return a.key < b.key; });
        return c;
    }
};

struct SpillStage : ff::ff_node_t<Chunk, RunInfo>
{
    std::string tmpdir;
    std::atomic<std::uint64_t> counter{0};
    explicit SpillStage(std::string td) : tmpdir(std::move(td))
    {
        std::filesystem::create_directories(tmpdir);
    }
    RunInfo *svc(Chunk *c) override
    {
        const auto id = counter.fetch_add(1, std::memory_order_relaxed);
        const std::string path = tmpdir + "/run_" + std::to_string(id) + ".bin";
        std::ofstream out(path, std::ios::binary);
        if (!out)
        {
            delete c;
            throw std::runtime_error("Cannot open " + path);
        }
        for (auto &it : *c)
            recordHeader::write_record(out, it.key, it.payload);
        auto *ri = new RunInfo{path, c->size()};
        delete c;
        return ri;
    }
};

// final merge helpers (local reader)
struct LReader
{
    std::ifstream in;
    bool eof{false};
    std::uint64_t key{};
    std::vector<std::uint8_t> payload;
    explicit LReader(const std::string &p) : in(p, std::ios::binary)
    {
        if (!in)
            throw std::runtime_error("Cannot open " + p);
        next();
    }
    bool next()
    {
        if (!recordHeader::read_record(in, key, payload))
        {
            eof = true;
            return false;
        }
        return true;
    }
};

struct CollectAndFinalMerge : ff::ff_node_t<RunInfo, void>
{
    std::string outpath;
    std::vector<std::string> runs;
    explicit CollectAndFinalMerge(std::string out) : outpath(std::move(out)) {}
    void *svc(RunInfo *r) override
    {
        runs.push_back(std::move(r->path));
        delete r;
        return GO_ON;
    }
    void eosnotify(ssize_t) override
    {
        if (runs.empty())
        {
            spdlog::warn("No runs to merge.");
            return;
        }
        struct Node
        {
            std::uint64_t k;
            size_t i;
        };
        struct Cmp
        {
            bool operator()(const Node &a, const Node &b) const { return a.k > b.k; }
        };
        std::vector<std::unique_ptr<LReader>> R;
        R.reserve(runs.size());
        for (auto &p : runs)
            R.emplace_back(std::make_unique<LReader>(p));
        std::priority_queue<Node, std::vector<Node>, Cmp> pq;
        for (size_t i = 0; i < R.size(); ++i)
            if (!R[i]->eof)
                pq.push(Node{R[i]->key, i});
        std::ofstream out(outpath, std::ios::binary);
        if (!out)
            throw std::runtime_error("Cannot open output " + outpath);
        size_t written = 0;
        while (!pq.empty())
        {
            auto n = pq.top();
            pq.pop();
            auto &rd = *R[n.i];
            recordHeader::write_record(out, rd.key, rd.payload);
            ++written;
            if (rd.next())
                pq.push(Node{rd.key, n.i});
        }
        spdlog::info("Final merge wrote {} records -> {}", written, outpath);
        for (auto &p : runs)
        {
            std::error_code ec;
            std::filesystem::remove(p, ec);
        }
        runs.clear();
    }
};

// ---------- driver with MEMORY_CAP gate, but pipelines only
static void usage(const char *prog)
{
    fmt::print(stderr,
               "Usage: {} <input.bin> <output.bin> <tmpdir> [MEM_CAP_MiB=2048] [chunkMiB=16]\n", prog);
}

int main(int argc, char **argv)
{
    try
    {
        if (argc < 4)
        {
            usage(argv[0]);
            return 1;
        }
        const std::string inpath = argv[1];
        const std::string outpath = argv[2];
        const std::string tmpdir = argv[3];
        const std::uint64_t MEM_CAP_MiB = (argc >= 5) ? std::stoull(argv[4]) : DEFAULT_MEMORY_CAP_MIB;
        const std::uint64_t chunkMiB = (argc >= 6) ? std::stoull(argv[5]) : DEFAULT_CHUNK_MIB;

        const auto fsz = std::filesystem::file_size(inpath);
        spdlog::info("Pure Pipeline | file={} bytes (~{:.2f} MiB) | MEM_CAP={} MiB | chunk={} MiB",
                     fsz, fsz / 1024.0 / 1024.0, MEM_CAP_MiB, chunkMiB);

        const std::uint64_t capB = MEM_CAP_MiB * 1024ULL * 1024ULL;

        if (fsz < capB)
        {
            // ------ in-memory pure pipeline: ReaderAll -> Sorter -> WriterAll
            ReaderAll r(inpath);
            SortStage_IV s;
            WriterAll w(outpath);
            ff::ff_Pipe pipe(r, s, w);
            if (pipe.run_and_wait_end() < 0)
            {
                spdlog::error("Pipeline failed (in-memory)");
                return 2;
            }
        }
        else
        {
            // ------ out-of-core pure pipeline: ReaderChunks -> Sorter -> Spiller -> Collector(merge on EOS)
            ReaderChunks rc(inpath, chunkMiB);
            SortStage_OC s;
            SpillStage sp(tmpdir);
            CollectAndFinalMerge c(outpath);
            ff::ff_Pipe pipe(rc, s, sp, c);
            if (pipe.run_and_wait_end() < 0)
            {
                spdlog::error("Pipeline failed (OOC)");
                return 3;
            }
        }

        spdlog::info("Done.");
        return 0;
    }
    catch (const std::exception &e)
    {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }
}