#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <iomanip>

// Same CompactPayload from the actual code
class CompactPayload
{
private:
    uint8_t *data_;

public:
    CompactPayload() : data_(nullptr) {}

    ~CompactPayload()
    {
        if (data_)
            delete[] data_;
    }

    CompactPayload(const CompactPayload &other)
    {
        if (other.data_)
        {
            uint32_t sz = other.size();
            data_ = new uint8_t[4 + sz];
            std::memcpy(data_, other.data_, 4 + sz);
        }
        else
        {
            data_ = nullptr;
        }
    }

    CompactPayload(CompactPayload &&other) noexcept : data_(other.data_)
    {
        other.data_ = nullptr;
    }

    CompactPayload &operator=(CompactPayload &&other) noexcept
    {
        if (this != &other)
        {
            if (data_)
                delete[] data_;
            data_ = other.data_;
            other.data_ = nullptr;
        }
        return *this;
    }

    void resize(size_t new_size)
    {
        if (data_)
            delete[] data_;
        data_ = new uint8_t[4 + new_size];
        *reinterpret_cast<uint32_t *>(data_) = static_cast<uint32_t>(new_size);
    }

    uint32_t size() const
    {
        return data_ ? *reinterpret_cast<const uint32_t *>(data_) : 0;
    }

    uint8_t *data()
    {
        return data_ ? data_ + 4 : nullptr;
    }
};

struct Item
{
    uint64_t key;
    CompactPayload payload;
};

// Same calculation from ooc_omp_v3.cpp
uint64_t record_size_bytes(const Item &item)
{
    return sizeof(uint64_t) + sizeof(uint32_t) + item.payload.size();
}

// Helper to get memory usage
size_t get_memory_usage()
{
    std::ifstream status_file("/proc/self/status");
    std::string line;
    while (std::getline(status_file, line))
    {
        if (line.substr(0, 6) == "VmRSS:")
        {
            std::string value_str;
            for (char c : line.substr(6))
            {
                if (std::isdigit(c))
                    value_str += c;
            }
            if (!value_str.empty())
            {
                return std::stoull(value_str) * 1024; // Convert KB to bytes
            }
        }
    }
    return 0;
}

void print_memory(const std::string &label, size_t mem_bytes)
{
    double mem_mb = mem_bytes / (1024.0 * 1024.0);
    double mem_gb = mem_bytes / (1024.0 * 1024.0 * 1024.0);
    std::cout << std::fixed << std::setprecision(2);
    std::cout << label << mem_mb << " MB (" << mem_gb << " GB)" << std::endl;
}

int main()
{
    std::cout << "\n=== Memory Overhead Test ===\n"
              << std::endl;

    const size_t NUM_ITEMS = 100'000'000; // 100M items
    const size_t PAYLOAD_SIZE = 32;       // 24 bytes payload

    std::cout << "Test parameters:" << std::endl;
    std::cout << "  - Number of items: " << NUM_ITEMS << std::endl;
    std::cout << "  - Payload size: " << PAYLOAD_SIZE << " bytes" << std::endl;
    std::cout << std::endl;

    size_t mem_before = get_memory_usage();
    print_memory("Memory before allocation: ", mem_before);

    // Allocate vector
    std::vector<Item> items;
    items.reserve(NUM_ITEMS);

    size_t mem_after_reserve = get_memory_usage();
    print_memory("Memory after reserve: ", mem_after_reserve);

    // Calculate expected size using record_size_bytes logic
    uint64_t expected_bytes = NUM_ITEMS * (sizeof(uint64_t) + sizeof(uint32_t) + PAYLOAD_SIZE);
    double expected_gb = expected_bytes / (1024.0 * 1024.0 * 1024.0);
    std::cout << "\nExpected by record_size_bytes(): " << expected_bytes
              << " bytes (" << std::fixed << std::setprecision(3) << expected_gb << " GB)" << std::endl;

    // Actually allocate items
    std::cout << "\nAllocating 100M items..." << std::endl;
    uint64_t accumulated = 0;

    for (size_t i = 0; i < NUM_ITEMS; ++i)
    {
        Item item;
        item.key = i;
        item.payload.resize(PAYLOAD_SIZE);

        // Fill with some data to prevent optimization
        for (size_t j = 0; j < PAYLOAD_SIZE; ++j)
        {
            item.payload.data()[j] = static_cast<uint8_t>(i % 256);
        }

        accumulated += record_size_bytes(item);
        items.push_back(std::move(item));

        // Print progress every 10M items
        if ((i + 1) % 10'000'000 == 0)
        {
            size_t current_mem = get_memory_usage();
            std::cout << "  Items: " << (i + 1) / 1'000'000 << "M, ";
            std::cout << "Accumulated: " << std::fixed << std::setprecision(2)
                      << accumulated / (1024.0 * 1024.0 * 1024.0) << " GB, ";
            print_memory("Actual memory: ", current_mem);
        }
    }

    size_t mem_after = get_memory_usage();

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Accumulated by record_size_bytes(): "
              << std::fixed << std::setprecision(3)
              << accumulated / (1024.0 * 1024.0 * 1024.0) << " GB" << std::endl;

    print_memory("Actual memory used: ", mem_after);

    size_t memory_diff = mem_after - mem_before;
    print_memory("Net memory increase: ", memory_diff);

    // Calculate overhead
    double overhead_percent = ((double)memory_diff - (double)expected_bytes) / (double)expected_bytes * 100.0;
    std::cout << "\nMemory overhead: " << std::fixed << std::setprecision(1)
              << overhead_percent << "%" << std::endl;

    // Calculate actual per-item memory
    double actual_per_item = (double)memory_diff / (double)NUM_ITEMS;
    double expected_per_item = (double)expected_bytes / (double)NUM_ITEMS;

    std::cout << "\nPer-item analysis:" << std::endl;
    std::cout << "  Expected per item: " << std::fixed << std::setprecision(1)
              << expected_per_item << " bytes" << std::endl;
    std::cout << "  Actual per item: " << actual_per_item << " bytes" << std::endl;
    std::cout << "  Overhead per item: " << (actual_per_item - expected_per_item) << " bytes" << std::endl;

    std::cout << "\n=== Breakdown ===" << std::endl;
    std::cout << "Expected structure:" << std::endl;
    std::cout << "  - Key (uint64_t): 8 bytes" << std::endl;
    std::cout << "  - Size (uint32_t): 4 bytes" << std::endl;
    std::cout << "  - Payload data: " << PAYLOAD_SIZE << " bytes" << std::endl;
    std::cout << "  - Total per record_size_bytes: " << expected_per_item << " bytes" << std::endl;

    std::cout << "\nActual in-memory structure:" << std::endl;
    std::cout << "  - Item struct (key + pointer): " << sizeof(Item) << " bytes" << std::endl;
    std::cout << "  - CompactPayload heap allocation: " << (4 + PAYLOAD_SIZE) << " bytes" << std::endl;
    std::cout << "  - Allocator metadata: ~8-16 bytes" << std::endl;
    std::cout << "  - Vector capacity overhead: varies" << std::endl;
    std::cout << "  - Total actual: ~" << actual_per_item << " bytes" << std::endl;

    return 0;
}
