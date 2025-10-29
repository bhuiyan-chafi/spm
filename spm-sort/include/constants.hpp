#pragma once
#include <iostream>
#include <string>
/**
 * Constants:
 * @param DATA_IN_STREAM is the input data stream file path
 * @param DATA_OUT_STREAM is the output data stream file path
 * @param DATA_TMP_DIR is the temporary directory for intermediate files
 */

const std::string DATA_IN_STREAM = "../data/records_7GiB.bin";
const std::string DATA_OUT_STREAM = "../data/sorted_records.bin";
const std::string DATA_TMP_DIR = "../data/tmp/";
const uint64_t MEMORY_CAP = 2'147'483'648ULL; // 2 GiB
// const uint64_t MEMORY_CAP = 4'294'967'296ULL; // 4 GiB
//   used to display input size in MiB/GiB
const uint64_t SMALL_DATA_SIZE = 1'073'741'824ULL; // 1GiB
