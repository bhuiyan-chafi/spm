#include "main.hpp"

/**
 * ------ Processors ------
 */
void parse_cli_and_set(int argc, char **argv)
{

    if (argc < 1 || argc > 5)
    {
        spdlog::error("Program accepts exactly 2/3/5 parameters, please read the instructions properly!");
        throw std::runtime_error("");
    }
    if (argc == 2)
    {
        DATA_INPUT = argv[1];
    }
    else if (argc == 3)
    {
        // generator
        std::string records_in_string = argv[1];
        std::string payload = argv[2];
        PAYLOAD_MAX = std::stoull(payload);
        if (!PAYLOAD_MAX)
            PAYLOAD_MAX = 256;
        if (records_in_string == "1M")
        {
            RECORDS = 10'000'000ULL;
            DATA_INPUT = "../data/rec_" + records_in_string + "_" + payload + ".bin";
        }
        else if (records_in_string == "5M")
        {
            RECORDS = 50'000'000ULL;
            DATA_INPUT = "../data/rec_" + records_in_string + "_" + payload + ".bin";
        }
        else if (records_in_string == "10M")
        {

            RECORDS = 100'000'000ULL;
            DATA_INPUT = "../data/rec_" + records_in_string + "_" + payload + ".bin";
        }
        else
        {
            spdlog::error("Records must be: 1M/5M/10M, you provided: {}", records_in_string);
        }
    }
    else
    {
        std::string records_in_string = argv[1];
        std::string payload = argv[2];
        std::string workers = argv[3];
        std::string memory = argv[4];
        if (records_in_string == "1M")
        {
            RECORDS = 10'000'000ULL;
            DATA_INPUT = "../data/rec_" + records_in_string + "_" + payload + ".bin";
        }
        else if (records_in_string == "5M")
        {
            RECORDS = 50'000'000ULL;
            DATA_INPUT = "../data/rec_" + records_in_string + "_" + payload + ".bin";
        }
        else if (records_in_string == "10M")
        {

            RECORDS = 100'000'000ULL;
            DATA_INPUT = "../data/rec_" + records_in_string + "_" + payload + ".bin";
        }
        else
        {
            spdlog::error("Records must be: 1M/5M/10M, you provided: {}", records_in_string);
        }
        MEMORY_CAP = std::stoull(memory) * IN_GB;
        if (!MEMORY_CAP)
            MEMORY_CAP = IN_GB;
        WORKERS = std::stoull(workers);
        if (!WORKERS)
            WORKERS = 1; // we can do ff_numCores but let's not accidentally take all cores
        PAYLOAD_MAX = std::stoull(payload);
        if (!PAYLOAD_MAX)
            PAYLOAD_MAX = 256;
        spdlog::info("==> Calculating INPUT_SIZE <==");
        INPUT_BYTES = estimate_stream_size();
        //  example: 1GB/4GB = 1GB -> (1 * 1GB) / 4(workers) = 256MiB buffer to read
        DISTRIBUTION_CAP = std::max(1UL, (INPUT_BYTES / MEMORY_CAP));
        DISTRIBUTION_CAP = (DISTRIBUTION_CAP * IN_GB) / WORKERS;
        spdlog::info("==> Parameters : IN_PATH={}, IN_SIZE: {} GiB, M_CAP={} GiB, W={} <==", DATA_INPUT, INPUT_BYTES / (1024.0 * 1024.0 * 1024.0), memory, WORKERS);
    }
}

void write_record(std::ostream &stream_out, uint64_t key, const std::vector<uint8_t> &payload)
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

bool read_record(std::istream &stream_in, uint64_t &key_out,
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

void load_all_data_in_memory(std::vector<Item> &items, std::ifstream &in)
{
    while (true)
    {
        uint64_t key;
        std::vector<uint8_t> payload;
        if (!read_record(in, key, payload))
        {
            break; // EOF
        }
        items.push_back(Item{key, std::move(payload)});
    }
}

void write_temp_slice(const std::string &temp_slice_out_path, const std::vector<Item> &temp_item_slice)
{
    std::ofstream out(temp_slice_out_path, std::ios::binary);
    if (!out)
        throw std::runtime_error("Data stream error: cannot open input/output file.");
    for (const auto &item : temp_item_slice)
    {
        write_record(out, item.key, item.payload);
    }
}

/**
 * calculate stream size to decide where(in/out bound of memory) to process the data
 */
uint64_t estimate_stream_size()
{
    namespace fs = std::filesystem;
    std::error_code error_code;
    spdlog::info("DATA_INPUT: {} to calculate size", DATA_INPUT);
    auto stream_size = fs::file_size(DATA_INPUT, error_code);
    if (!error_code)
    {
        stream_size = static_cast<uint64_t>(stream_size);
        if (stream_size < IN_GB)
        {
            spdlog::info("-> INPUT DATA SIZE: {} MiB", stream_size / (double)(1024.0 * 1024.0));
            return stream_size;
        }
        else
        {
            spdlog::info("-> INPUT DATA SIZE: {} GiB", stream_size / (double)(1024.0 * 1024.0 * 1024.0));
            return stream_size;
        }
    }
    spdlog::error("==> X => Error in ESTIMATING_STREAM_SIZE <==");
    throw std::runtime_error("");
}

/**
 * sorting in Memory Function: sort_in_memory(), and the stream is for sure < MEMORY_CAP
 */
void sort_in_memory()
{
    TimerClass sorting_time;
    TimerClass writing_time;
    std::ifstream in(DATA_INPUT, std::ios::binary);
    std::ofstream out(DATA_OUTPUT, std::ios::binary);
    if (!in || !out)
    {
        spdlog::error("==> X Error in in/out DATA_STREAM X <==");
        throw std::runtime_error("");
    }
    std::vector<Item> items;
    {
        TimerScope st(sorting_time);
        load_all_data_in_memory(items, in);
        spdlog::info("-> Total INPUT quantity: {}, starting MERGE_SORT", items.size());
        std::sort(items.begin(), items.end(),
                  [](const Item &a, const Item &b)
                  { return a.key < b.key; });
    }
    spdlog::info("->[Timer:Sort] IN_MEMORY_SORT: {}", sorting_time.result());
    {
        TimerScope wt(writing_time);
        spdlog::info("-> Writing results");
        for (const auto &it : items)
        {
            write_record(out, it.key, it.payload);
        }
    }
    spdlog::info("->[Timer:Write] IN_MEMORY_SORT: {}", writing_time.result());
    in.close();
    out.close();
}

/**
 * sort in slices, out of memory capacity(MEMORY_CAP)
 */
void sort_out_of_core()
{
    TimerClass slice_sort_write_time;
    TimerClass merge_write_time;
    std::ifstream in(DATA_INPUT, std::ios::binary);
    std::ofstream out(DATA_OUTPUT, std::ios::binary);

    if (!in || !out)
    {
        spdlog::error("==> X Error in in/out DATA_STREAM X <==");
        throw std::runtime_error("");
    }

    std::vector<std::string> temp_record_paths;
    std::vector<Item> temp_items;
    uint64_t current_stream_size_inBytes = 0ULL;
    uint64_t current_stream_index = 0;
    uint64_t current_item_size = 0ULL;

    while (true)
    {
        TimerScope sswt(slice_sort_write_time);
        uint64_t key;
        std::vector<uint8_t> payload;
        if (!read_record(in, key, payload))
        {
            if (!temp_items.empty())
            {
                std::sort(temp_items.begin(), temp_items.end(),
                          [](const Item &a, const Item &b)
                          { return a.key < b.key; });
                std::string temp_slice_out_path = DATA_TMP_DIR + "run_" + std::to_string(current_stream_index) + ".bin";
                write_temp_slice(temp_slice_out_path, temp_items);
                temp_record_paths.push_back(temp_slice_out_path);
                // free the memory and reset
                std::vector<Item>().swap(temp_items);
            }
            break;
        }
        current_item_size = sizeof(uint64_t) + sizeof(uint32_t) + payload.size();
        if ((current_stream_size_inBytes + current_item_size) > MEMORY_CAP)
        {
            std::sort(temp_items.begin(), temp_items.end(),
                      [](const Item &a, const Item &b)
                      { return a.key < b.key; });
            std::string temp_slice_out_path = DATA_TMP_DIR + "run_" + std::to_string(current_stream_index++) + ".bin";
            write_temp_slice(temp_slice_out_path, temp_items);
            temp_record_paths.push_back(temp_slice_out_path);
            // free the memory and reset
            std::vector<Item>().swap(temp_items);
            current_stream_size_inBytes = 0ULL;
        }
        temp_items.push_back(Item{key, std::move(payload)});
        current_stream_size_inBytes += current_item_size;
    }
    spdlog::info("->[Timer:Slice->Sort->Write] MEMORY_OOB: {}", slice_sort_write_time.result());
    /**
     *  ------------------ Perform k-way merge -------------------
     *  ------------- accumulate slices and produce one file --------------
     */
    {
        TimerScope mwt(merge_write_time);
        std::vector<std::unique_ptr<TempReader>> readers;
        readers.reserve(temp_record_paths.size());
        for (const auto &record_path : temp_record_paths)
            readers.push_back(std::make_unique<TempReader>(record_path));
        std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> heap;
        for (uint64_t i = 0; i < readers.size(); ++i)
        {
            if (!readers[i]->eof)
                heap.push(HeapNode{readers[i]->key, i});
        }
        while (!heap.empty())
        {
            auto top = heap.top();
            heap.pop();
            auto &current_reader = *readers[top.temp_run_index];
            write_record(out, current_reader.key, current_reader.payload);
            current_reader.advance();
            if (!current_reader.eof)
            {
                heap.push(HeapNode{current_reader.key, top.temp_run_index});
            }
        }
    }
    spdlog::info("->[Timer:Merge->Write] MEMORY_OOB: {}", merge_write_time.result());
    for (const auto &temp_record_path : temp_record_paths)
    {
        std::remove(temp_record_path.c_str());
    }
    in.close();
    out.close();
}