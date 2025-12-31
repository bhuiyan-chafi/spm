#include <iostream>
#include <unistd.h> // Required for sysconf()
#include <iomanip>  // For std::setw (to align output)

int main()
{
    // --- Level 1 (L1) Cache ---
    long l1_data_cache_size = sysconf(_SC_LEVEL1_DCACHE_SIZE);
    long l1_instruction_cache_size = sysconf(_SC_LEVEL1_ICACHE_SIZE);
    long l1_data_cache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

    std::cout << "--- Level 1 (L1) Cache ---" << std::endl;

    if (l1_data_cache_size != -1)
    {
        std::cout << "L1 Data Cache Size:        " << std::setw(6)
                  << (l1_data_cache_size / 1024) << " KB ("
                  << l1_data_cache_size << " bytes)" << std::endl;
    }

    if (l1_instruction_cache_size != -1)
    {
        std::cout << "L1 Instruction Cache Size: " << std::setw(6)
                  << (l1_instruction_cache_size / 1024) << " KB ("
                  << l1_instruction_cache_size << " bytes)" << std::endl;
    }

    if (l1_data_cache_line_size != -1)
    {
        std::cout << "L1 Data Cache Line Size:   " << std::setw(6)
                  << l1_data_cache_line_size << " bytes" << std::endl;
    }

    // --- Level 2 (L2) Cache ---
    long l2_cache_size = sysconf(_SC_LEVEL2_CACHE_SIZE);
    long l2_cache_line_size = sysconf(_SC_LEVEL2_CACHE_LINESIZE);

    std::cout << "\n--- Level 2 (L2) Cache ---" << std::endl;

    if (l2_cache_size != -1)
    {
        // L2 is often much larger, so let's show KB
        std::cout << "L2 Unified Cache Size:     " << std::setw(6)
                  << (l2_cache_size / 1024) << " KB ("
                  << l2_cache_size << " bytes)" << std::endl;
    }
    else
    {
        std::cout << "Could not determine L2 Cache size." << std::endl;
    }

    if (l2_cache_line_size != -1)
    {
        std::cout << "L2 Cache Line Size:        " << std::setw(6)
                  << l2_cache_line_size << " bytes" << std::endl;
    }

    return 0;
}