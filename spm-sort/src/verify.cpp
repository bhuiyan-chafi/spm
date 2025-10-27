#include "record.hpp"
#include "spdlog/spdlog.h"
#include "constants.hpp"
#include "../../utils/microtimer.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <limits>
#include <omp.h>

using u64 = std::uint64_t;
using u32 = std::uint32_t;

// ---- 64-bit FNV-1a over key,len,payload (same as yours) ----
static u64 fnv1a64(const u64 key, const u32 len, const std::vector<uint8_t> &payload)
{
    const u64 FNV_OFFSET = 1469598103934665603ull;
    const u64 FNV_PRIME = 1099511628211ull;
    auto mix = [&](uint8_t b, u64 &h)
    { h ^= b; h *= FNV_PRIME; };

    u64 h = FNV_OFFSET;
    for (int i = 0; i < 8; ++i)
        mix(static_cast<uint8_t>((key >> (8 * i)) & 0xFF), h);
    for (int i = 0; i < 4; ++i)
        mix(static_cast<uint8_t>((len >> (8 * i)) & 0xFF), h);
    for (uint8_t b : payload)
        mix(b, h);
    return h;
}

struct Fingerprint
{
    u64 count = 0;
    u64 sum_payload = 0;
    u64 xor_hash = 0;
    // keep a wide accumulator to combine partial sums robustly; we'll compare low 64b
    uint64_t sum_hash = 0;
};

// -------- Pass-1: compute chunk starts aligned to record boundaries --------
static bool compute_chunk_starts(const std::string &path, int T,
                                 std::vector<std::streampos> &starts,
                                 std::string &err)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        err = "cannot open " + path;
        return false;
    }

    in.seekg(0, std::ios::end);
    const std::streampos file_end = in.tellg();
    in.seekg(0, std::ios::beg);

    starts.clear();
    starts.reserve(T + 1);
    starts.push_back(0); // first chunk

    // byte targets for near-even splits; we will align to next record boundary
    std::vector<std::streampos> targets;
    targets.reserve(T - 1);
    for (int i = 1; i < T; ++i)
    {
        targets.push_back(static_cast<std::streampos>((uint64_t)file_end * i / T));
    }
    size_t tgt = 0;

    while (in.tellg() < file_end)
    {
        std::streampos before = in.tellg();

        u64 key = 0;
        std::vector<uint8_t> payload;
        if (!recordHeader::read_record(in, key, payload))
            break; // EOF or parse error

        std::streampos after = in.tellg();
        // if we passed one or more targets, next chunk starts here (after the record)
        while (tgt < targets.size() && after >= targets[tgt])
        {
            starts.push_back(after);
            ++tgt;
        }
    }
    // ensure end as sentinel
    starts.push_back(file_end);

    // Normalize to exactly T+1 entries (handles tiny files)
    if (starts.size() < static_cast<size_t>(T + 1))
    {
        while (starts.size() < static_cast<size_t>(T + 1))
            starts.push_back(file_end);
    }
    else if (starts.size() > static_cast<size_t>(T + 1))
    {
        starts.resize(T);
        starts.push_back(file_end);
    }
    return true;
}

// -------- Pass-2: parallel fingerprint over disjoint [start, end) byte ranges --------
static bool fingerprint_file_parallel(const std::string &path,
                                      Fingerprint &fp,
                                      bool check_sorted,
                                      std::string &err)
{
    const int T = std::max(1, omp_get_max_threads());

    std::vector<std::streampos> starts;
    if (!compute_chunk_starts(path, T, starts, err))
        return false;

    // per-thread boundary info for global sorted-ness
    std::vector<u64> first_key(T, 0), last_key(T, 0);
    std::vector<bool> has_any(T, false), chunk_sorted(T, true);

    // global reductions
    u64 g_count = 0, g_sum_payload = 0, g_xor = 0;
    uint64_t g_sum128 = 0;

#pragma omp parallel for reduction(+ : g_count, g_sum_payload) reduction(^ : g_xor)
    for (int ti = 0; ti < T; ++ti)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            chunk_sorted[ti] = false;
            continue;
        }
        in.seekg(starts[ti], std::ios::beg);

        u64 loc_count = 0, loc_sum_payload = 0, loc_xor = 0;
        uint64_t loc_sum128 = 0;
        bool first = true;
        u64 prev = 0;

        while (in && in.tellg() < starts[ti + 1])
        {
            std::streampos before = in.tellg();
            u64 key = 0;
            std::vector<uint8_t> payload;
            if (!recordHeader::read_record(in, key, payload))
                break;
            std::streampos after = in.tellg();

            // if this record would cross into next chunk, leave it to the next thread
            if (after > starts[ti + 1])
            {
                in.seekg(before);
                break;
            }

            u32 len = static_cast<u32>(payload.size());
            u64 h = fnv1a64(key, len, payload);

            ++loc_count;
            loc_sum_payload += len;
            loc_xor ^= h;
            loc_sum128 += static_cast<uint64_t>(h);

            if (check_sorted)
            {
                if (first)
                {
                    first_key[ti] = key;
                    prev = key;
                    has_any[ti] = true;
                    first = false;
                }
                else
                {
                    if (key < prev)
                    {
                        chunk_sorted[ti] = false;
                        break;
                    }
                    prev = key;
                }
                last_key[ti] = prev;
            }
        }

        g_count += loc_count;
        g_sum_payload += loc_sum_payload;
        g_xor ^= loc_xor;

#pragma omp critical
        {
            g_sum128 += loc_sum128;
        }
    }

    // cross-chunk boundary check
    if (check_sorted)
    {
        for (int i = 0; i < T; ++i)
        {
            if (!chunk_sorted[i])
            {
                err = "not sorted inside chunk " + std::to_string(i);
                return false;
            }
        }
        for (int i = 0; i + 1 < T; ++i)
        {
            if (has_any[i] && has_any[i + 1])
            {
                if (last_key[i] > first_key[i + 1])
                {
                    err = "not sorted at chunk boundary " + std::to_string(i) +
                          " (" + std::to_string(last_key[i]) + " -> " + std::to_string(first_key[i + 1]) + ")";
                    return false;
                }
            }
        }
    }

    fp.count = g_count;
    fp.sum_payload = g_sum_payload;
    fp.xor_hash = g_xor;
    fp.sum_hash = g_sum128; // store wide sum; main will use low 64b
    return true;
}

// ------------------------------ MAIN ------------------------------
int main()
{
    Fingerprint input_bin, output_bin;
    std::string err;

    spdlog::info("==> PHASE: 1 -> Process has been started.....");
    spdlog::info("==> PHASE: 2 -> Original: fingerprint only");
    if (!fingerprint_file_parallel(DATA_IN_STREAM, input_bin, /*check_sorted=*/false, err))
    {
        spdlog::error("Original: {}", err);
        return 1;
    }

    spdlog::info("==> PHASE: 3 -> Sorted: fingerprint + non-decreasing check");
    if (!fingerprint_file_parallel(DATA_OUT_STREAM, output_bin, /*check_sorted=*/true, err))
    {
        spdlog::error("Sorted: {}", err);
        return 1;
    }

    spdlog::info("==> PHASE: 4 -> Compare fingerprints (use low 64b of the 128-bit sum to match your previous logic)");
    const u64 sum_hash_lo_orig = static_cast<u64>(input_bin.sum_hash);
    const u64 sum_hash_lo_sorted = static_cast<u64>(output_bin.sum_hash);

    bool ok = true;
    if (input_bin.count != output_bin.count)
    {
        spdlog::error("Count mismatch: orig={} sorted={}", input_bin.count, output_bin.count);
        ok = false;
    }
    if (input_bin.sum_payload != output_bin.sum_payload)
    {
        spdlog::error("Payload-bytes mismatch: orig={} sorted={}", input_bin.sum_payload, output_bin.sum_payload);
        ok = false;
    }
    if (input_bin.xor_hash != output_bin.xor_hash)
    {
        spdlog::error("XOR-hash mismatch: orig={} sorted={}", input_bin.xor_hash, output_bin.xor_hash);
        ok = false;
    }
    if (sum_hash_lo_orig != sum_hash_lo_sorted)
    {
        spdlog::error("SUM-hash mismatch: orig={} sorted={}", sum_hash_lo_orig, sum_hash_lo_sorted);
        ok = false;
    }

    if (!ok)
        return 1;

    spdlog::info("Sum check completed. Records: {} | Payload bytes: {} | xor=0x{:x} | sum_lo=0x{:x}",
                 output_bin.count, output_bin.sum_payload, output_bin.xor_hash, sum_hash_lo_sorted);
    return 0;
}