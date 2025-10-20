// src/verify.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include "record.hpp"
#include "spdlog/spdlog.h"

using u64 = std::uint64_t;
using u32 = std::uint32_t;
const std::string DATA_IN_STREAM = "../data/records.bin";
const std::string DATA_OUT_STREAM = "../data/sorted_records.bin";

// 64-bit FNV-1a over key,len,payload
static u64 fnv1a64(const u64 key, const u32 len, const std::vector<uint8_t> &payload)
{
    const u64 FNV_OFFSET = 1469598103934665603ull;
    const u64 FNV_PRIME = 1099511628211ull;
    auto mix = [&](uint8_t b, u64 &h)
    { h ^= b; h *= FNV_PRIME; };

    u64 h = FNV_OFFSET;

    // key (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i)
        mix(static_cast<uint8_t>((key >> (8 * i)) & 0xFF), h);
    // len (4 bytes)
    for (int i = 0; i < 4; ++i)
        mix(static_cast<uint8_t>((len >> (8 * i)) & 0xFF), h);
    // payload
    for (uint8_t b : payload)
        mix(b, h);
    return h;
}

struct Fingerprint
{
    u64 count = 0;
    u64 sum_payload = 0;
    u64 xor_hash = 0;
    std::uint64_t sum_hash = 0;
};

// Stream a file and compute fingerprint; if check_sorted=true, also verify non-decreasing keys.
static bool fingerprint_file(const std::string &path, Fingerprint &fp, bool check_sorted, std::string &err)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        err = "cannot open " + path;
        return false;
    }

    u64 prev_key = 0;
    bool first = true;

    while (true)
    {
        u64 key = 0;
        std::vector<uint8_t> payload;
        if (!recordHeader::read_record(in, key, payload))
            break; // EOF

        u32 len = static_cast<u32>(payload.size());
        const u64 h = fnv1a64(key, len, payload);

        ++fp.count;
        fp.sum_payload += len;
        fp.xor_hash ^= h;
        fp.sum_hash += h;

        if (check_sorted)
        {
            if (!first && key < prev_key)
            {
                err = "not sorted at record #" + std::to_string(fp.count - 1) +
                      " (" + std::to_string(prev_key) + " -> " + std::to_string(key) + ")";
                return false;
            }
            prev_key = key;
        }
        first = false;
    }
    return true;
}

int main()
{
    Fingerprint input_bin, output_bin;
    std::string err;

    // 1) Original: just fingerprint (order may be arbitrary)
    if (!fingerprint_file(DATA_IN_STREAM, input_bin, /*check_sorted=*/false, err))
    {
        spdlog::error("Original: {}", err);
        return 1;
    }

    // 2) Sorted: fingerprint + enforce non-decreasing keys
    if (!fingerprint_file(DATA_OUT_STREAM, output_bin, /*check_sorted=*/true, err))
    {
        spdlog::error("Sorted: {}", err);
        return 1;
    }

    // 3) Compare fingerprints
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