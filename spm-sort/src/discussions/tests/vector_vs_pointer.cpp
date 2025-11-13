#include <iostream>
#include <cstdint>
#include <vector>

class CompactPayload
{
private:
    uint8_t *data_;

public:
    CompactPayload() : data_(nullptr) {}
};

int main()
{
    std::cout << "sizeof(uint8_t*) = " << sizeof(uint8_t *) << " bytes" << std::endl;
    std::cout << "sizeof(CompactPayload) = " << sizeof(CompactPayload) << " bytes" << std::endl;
    std::cout << "sizeof(void*) = " << sizeof(void *) << " bytes" << std::endl;
    std::cout << "sizeof(std::vector<uint8_t>) = " << sizeof(std::vector<uint8_t>) << " bytes" << std::endl;

    std::cout << "- For 100M items using CompactPayload: " << (100000000ULL * sizeof(CompactPayload) / 1024 / 1024) << " MB" << std::endl;
    std::cout << "- For 100M items using vector: " << (100000000ULL * sizeof(std::vector<uint8_t>) / 1024 / 1024) << " MB" << std::endl;

    return 0;
}