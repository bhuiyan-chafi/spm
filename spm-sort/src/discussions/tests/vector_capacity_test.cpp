#include <iostream>
#include <vector>
#include <cstdint>

int main()
{
    std::vector<uint8_t> vec;

    const size_t iterations = 1'000'000;

    std::cout << "Starting test: pushing " << iterations << " uint8_t values\n";
    std::cout << "Expected size: " << iterations << " bytes (" << iterations / (1024.0 * 1024.0) << " MiB)\n\n";

    // Push without reserve
    for (size_t i = 0; i < iterations; ++i)
    {
        vec.push_back(static_cast<uint8_t>(i % 256));
    }

    size_t size_bytes = vec.size() * sizeof(uint8_t);
    size_t capacity_bytes = vec.capacity() * sizeof(uint8_t);

    std::cout << "Results:\n";
    std::cout << "  vec.size()     = " << vec.size() << " elements\n";
    std::cout << "  vec.capacity() = " << vec.capacity() << " elements\n";
    std::cout << "  Size in bytes     = " << size_bytes << " bytes (" << size_bytes / (1024.0 * 1024.0) << " MiB)\n";
    std::cout << "  Capacity in bytes = " << capacity_bytes << " bytes (" << capacity_bytes / (1024.0 * 1024.0) << " MiB)\n";
    std::cout << "  Overhead = " << (capacity_bytes - size_bytes) << " bytes (" << (capacity_bytes - size_bytes) / (1024.0 * 1024.0) << " MiB)\n";
    std::cout << "  Overhead percentage = " << ((capacity_bytes - size_bytes) * 100.0 / size_bytes) << "%\n";
    std::cout << "  Capacity/Size ratio = " << (static_cast<double>(capacity_bytes) / size_bytes) << "x\n";

    return 0;
}
