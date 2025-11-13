// compile: g++ -O3 vector_capacity_test.cpp -o vector_capacity_test
// run: ./vector_capacity_test

#include <iostream>
#include <vector>
#include <cstdint> // For uint64_t and uint8_t
#include <iomanip> // For formatting output

// The structure from your example
struct Item
{
    uint64_t key;
    std::vector<uint8_t> payload;
};

int main()
{
    // --- Part 1: Show the size of the components ---

    std::cout << "--- Component Sizes (on this 64-bit system) ---" << std::endl;

    // Size of the key
    std::cout << "Size of uint64_t (key):              "
              << sizeof(uint64_t) << " bytes" << std::endl;

    // This is the main culprit!
    // This is the size of the vector *object itself*, not the data it holds.
    std::cout << "Size of std::vector<uint8_t> (payload): "
              << sizeof(std::vector<uint8_t>) << " bytes" << std::endl;

    // Size of the whole struct. Should be 8 + 24 = 32 bytes.
    std::cout << "-------------------------------------------------" << std::endl;
    std::cout << "Total size of one 'Item' struct:      "
              << sizeof(Item) << " bytes" << std::endl;
    std::cout << std::endl;

    // --- Part 2: Calculate the overhead for 10 million records ---

    const uint64_t NUM_RECORDS = 10'000'000;
    const double BYTES_PER_MB = 1024.0 * 1024.0;

    // Calculate the overhead from *just* the vector objects
    uint64_t vector_overhead_bytes = NUM_RECORDS * sizeof(std::vector<uint8_t>);

    // Calculate the total memory for *all* the Item objects
    // This doesn't include the actual payload data they will point to!
    uint64_t total_struct_bytes = NUM_RECORDS * sizeof(Item);

    // Convert to megabytes for readability
    double vector_overhead_mb = static_cast<double>(vector_overhead_bytes) / BYTES_PER_MB;
    double total_struct_mb = static_cast<double>(total_struct_bytes) / BYTES_PER_MB;

    // Set precision for clean output
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "--- Overhead for 10,000,000 Records ---" << std::endl;

    std::cout << "Total overhead from 10M vector *objects*: "
              << vector_overhead_mb << " MB" << std::endl;

    std::cout << "Total memory for 10M 'Item' *structs*:    "
              << total_struct_mb << " MB" << std::endl;

    std::cout << "\nThis " << vector_overhead_mb << " MB is *pure overhead* before storing"
              << " a single byte of payload data!" << std::endl;

    return 0;
}