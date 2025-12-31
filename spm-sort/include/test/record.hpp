/**
 * @file record.hpp
 * Author: ASM CHAFIULLAH BHUIYAN, M.Sc. in Computer Science and Network, University of Pisa, Italy
 * Contact: a.bhuiyan@studenti.unipi.it
 * Created on: October 2025
 * Project: Project-1(Merge Sort) problem in Parallel and Distributed Systems course,
 * University of Pisa, Italy)
 * You are free to use, modify, and distribute this code for educational purposes.
 */

#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <limits>
#include <spdlog/spdlog.h>

namespace recordHeader
{
    /**
     * skipping the char[] payload;
     * because length of char[] payload is variable;
     * we need something fixed for the compiler;
     */
    struct Record
    {
        uint64_t key;
        uint32_t len;
    };

    /**
     *  Writing just one record: [u64 key][u32 len][len bytes payload]
     */
    inline void write_record(std::ostream &stream_out, uint64_t key, const std::vector<uint8_t> &payload)
    {
        if (!stream_out)
        {
            spdlog::error("Stream error while writing record: key={}", key);
            throw std::runtime_error("write_record: stream error");
        }
        Record record{key, static_cast<uint32_t>(payload.size())};
        // spdlog::info("writing_record: key={}, len={}", record.key, record.len);
        stream_out.write(reinterpret_cast<const char *>(&record), sizeof(record));
        stream_out.write(reinterpret_cast<const char *>(payload.data()), payload.size());
    }

    /**
     *  Reading just one record: [u64 key][u32 len][len bytes payload]
     */
    inline bool read_record(std::istream &stream_in, uint64_t &key_out,
                            std::vector<uint8_t> &payload_out)
    {
        Record record{};
        // this is one extra read, but this is just to test
        if ((!stream_in.read(reinterpret_cast<char *>(&record), sizeof(record))))
        {
            if (stream_in.eof())
                return false; // EOF reached, no more records
            throw std::runtime_error("Read stream error");
        }
        payload_out.resize(record.len);
        // we assumed the records are well-formed and in between 8 and 16 bytes
        stream_in.read(reinterpret_cast<char *>(payload_out.data()), record.len);
        key_out = record.key;
        return static_cast<bool>(stream_in); // true if payload read succeeded
    }
}