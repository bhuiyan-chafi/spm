#pragma once
#include <iostream>
#include <cstdint>
#include <string>
/**
 * Constants:
 * @param DATA_IN_STREAM is the input data stream file path
 * @param DATA_OUT_STREAM is the output data stream file path
 * @param DATA_TMP_DIR is the temporary directory for intermediate files
 */
inline constexpr const char *DATA_IN_STREAM_1GiB = "../data/records_1GiB.bin";
inline constexpr const char *DATA_IN_STREAM_3GiB = "../data/records_3GiB.bin";
inline constexpr const char *DATA_IN_STREAM_7GiB = "../data/records_7GiB.bin";
inline constexpr const char *DATA_IN_STREAM_19GiB = "../data/records_19GiB.bin";

const std::string DATA_OUT_STREAM = "../data/sorted_records.bin";
const std::string DATA_TMP_DIR = "../data/tmp/";

inline constexpr uint64_t MEMORY_CAP_2GiB = 2'147'483'648ULL;
inline constexpr uint64_t MEMORY_CAP_3GiB = 3'221'225'472ULL;
inline constexpr uint64_t MEMORY_CAP_4GiB = 4'294'967'296ULL;
inline constexpr uint64_t SMALL_DATA_SIZE = 1'073'741'824ULL;
// inline (single program-wide definition)
inline std::string DATA_IN_STREAM{""};
inline uint64_t MEMORY_CAP{0ULL};
inline uint64_t WORKERS{0};