// seq.cpp  -- Sequential baseline with simple out-of-core fallback (C++20)
// Build: g++ -std=c++20 -O3 -Wall -Wextra -pedantic seq.cpp -o seq
// Use : ./seq in.bin out.bin PAYLOAD_MAX [mem_limit_bytes] [tmp_dir]

#include <bits/stdc++.h>
#include "record.hpp" // <- your header; we DO NOT modify it

using namespace std;

// ---------------------------- Small helpers ---------------------------------

static uint64_t to_u64(const string &s)
{
    // very simple parser; accepts decimal only
    return std::stoull(s);
}

struct Item
{
    uint64_t key;
    std::vector<uint8_t> payload; // payload bytes
};

// Estimate memory usage for in-RAM vector (very rough, but enough to keep us safe)
static inline uint64_t est_bytes(const vector<Item> &v)
{
    uint64_t sum = 0;
    for (const auto &it : v)
    {
        // 8 bytes key + 4 bytes len + payload + small overhead (~8)
        sum += 8 + 4 + it.payload.size() + 8;
    }
    // also count vector overhead a bit
    sum += v.size() * 16;
    return sum;
}

// --------------------- Phase A: run generation (OOC) ------------------------

static string make_run_path(const string &tmp_dir, size_t idx)
{
    ostringstream oss;
    oss << tmp_dir << "/run_" << idx << ".bin";
    return oss.str();
}

// Spill a sorted run to disk
static void write_run(const string &path, const vector<Item> &run)
{
    ofstream out(path, ios::binary);
    if (!out)
        throw runtime_error("cannot open run file for write: " + path);
    for (const auto &it : run)
    {
        recordHeader::write_record(out, it.key, it.payload);
    }
}

// --------------------- Phase B: k-way merge (OOC) ---------------------------

struct RunReader
{
    ifstream in;
    bool eof = false;
    uint64_t key = 0;
    vector<uint8_t> payload;

    explicit RunReader(const string &path) : in(path, ios::binary)
    {
        if (!in)
            throw runtime_error("cannot open run for read: " + path);
        advance(); // prefetch first record
    }

    void advance()
    {
        if (!recordHeader::read_record(in, key, payload))
        {
            eof = true;
        }
    }
};

struct HeapNode
{
    uint64_t key;
    size_t run_idx; // which run this record came from
    // no payload here; we keep payload in the RunReader to avoid copying
    bool operator>(const HeapNode &other) const { return key > other.key; }
};

// ------------------------- Sorting strategies -------------------------------

// Try to read entire file into memory. If it exceeds mem_limit, return false.
static bool sort_in_memory(const string &in_path,
                           const string &out_path,
                           uint32_t PAYLOAD_MAX,
                           uint64_t mem_limit_bytes)
{
    ifstream in(in_path, ios::binary);
    if (!in)
        throw runtime_error("cannot open input file");

    vector<Item> items;
    items.reserve(1024); // small start

    // Phase: load
    while (true)
    {
        uint64_t key;
        vector<uint8_t> payload;
        if (!recordHeader::read_record(in, key, payload))
            break; // EOF
        items.push_back(Item{key, std::move(payload)});

        // Memory guard (very rough)
        if ((items.size() & 0xFFF) == 0)
        { // every ~4096 recs, check
            if (est_bytes(items) > mem_limit_bytes)
            {
                return false; // switch to OOC
            }
        }
    }

    // Phase: sort
    std::sort(items.begin(), items.end(),
              [](const Item &a, const Item &b)
              { return a.key < b.key; });

    // Phase: write
    ofstream out(out_path, ios::binary);
    if (!out)
        throw runtime_error("cannot open output file");
    for (const auto &it : items)
    {
        recordHeader::write_record(out, it.key, it.payload);
    }
    return true;
}

// External (single-thread) merge sort: run generation + k-way merge
static void sort_out_of_core(const string &in_path,
                             const string &out_path,
                             uint32_t PAYLOAD_MAX,
                             uint64_t mem_limit_bytes,
                             const string &tmp_dir)
{
    // ------------------ Phase A: make sorted runs -------------------
    ifstream in(in_path, ios::binary);
    if (!in)
        throw runtime_error("cannot open input file");

    vector<string> run_paths;
    vector<Item> run;
    run.reserve(1024);

    uint64_t current_bytes = 0;
    size_t run_idx = 0;

    while (true)
    {
        uint64_t key;
        vector<uint8_t> payload;
        if (!recordHeader::read_record(in, key, payload))
        {
            // EOF -> spill the last partial run if any
            if (!run.empty())
            {
                sort(run.begin(), run.end(),
                     [](const Item &a, const Item &b)
                     { return a.key < b.key; });
                string path = make_run_path(tmp_dir, run_idx++);
                write_run(path, run);
                run_paths.push_back(path);
                run.clear();
            }
            break;
        }

        // add to run
        current_bytes += 8 + 4 + payload.size() + 8; // rough estimate
        run.push_back(Item{key, std::move(payload)});

        // if we exceeded mem_limit, sort+spill
        if (current_bytes >= mem_limit_bytes)
        {
            sort(run.begin(), run.end(),
                 [](const Item &a, const Item &b)
                 { return a.key < b.key; });
            string path = make_run_path(tmp_dir, run_idx++);
            write_run(path, run);
            run_paths.push_back(path);

            run.clear();
            current_bytes = 0;
        }
    }

    // Special case: if we produced 0 runs (empty input), just truncate output
    if (run_paths.empty())
    {
        ofstream out(out_path, ios::binary); // empty file
        return;
    }

    // If we produced exactly one run and never overflowed memory,
    // we could rename/move it, but for simplicity we still do a 1-way "merge".
    // ------------------ Phase B: k-way merge ------------------------
    vector<unique_ptr<RunReader>> readers;
    readers.reserve(run_paths.size());
    for (const auto &p : run_paths)
        readers.push_back(make_unique<RunReader>(p));

    priority_queue<HeapNode, vector<HeapNode>, std::greater<HeapNode>> heap;
    for (size_t i = 0; i < readers.size(); ++i)
    {
        if (!readers[i]->eof)
            heap.push(HeapNode{readers[i]->key, i});
    }

    ofstream out(out_path, ios::binary);
    if (!out)
        throw runtime_error("cannot open output file");

    while (!heap.empty())
    {
        auto top = heap.top();
        heap.pop();
        auto &rr = *readers[top.run_idx];

        // write current record
        recordHeader::write_record(out, rr.key, rr.payload);

        // advance that run
        rr.advance();
        if (!rr.eof)
        {
            heap.push(HeapNode{rr.key, top.run_idx});
        }
    }

    // cleanup temp runs
    for (const auto &p : run_paths)
    {
        std::remove(p.c_str());
    }
}

// --------------------------------- main -------------------------------------

int main(int argc, char **argv)
{
    if (argc < 4 || argc > 6)
    {
        cerr << "use: " << argv[0]
             << " in.bin out.bin PAYLOAD_MAX [mem_limit_bytes] [tmp_dir]\n";
        return 1;
    }

    const string in_path = argv[1];
    const string out_path = argv[2];
    const uint32_t PAYLOAD_MAX = static_cast<uint32_t>(stoul(argv[3]));

    // defaults: conservative but simple
    uint64_t mem_limit = (argc >= 5) ? to_u64(argv[4]) : (uint64_t)8ULL * 1024ULL * 1024ULL * 1024ULL; // 8 GiB
    string tmp_dir = (argc == 6) ? string(argv[5]) : string("/tmp");

    // Try in-memory first. If it would exceed mem_limit, use OOC path.
    try
    {
        bool ok = sort_in_memory(in_path, out_path, PAYLOAD_MAX, mem_limit);
        if (!ok)
        {
            // fallback to external merge (still sequential)
            sort_out_of_core(in_path, out_path, PAYLOAD_MAX, mem_limit, tmp_dir);
        }
    }
    catch (const exception &e)
    {
        cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}