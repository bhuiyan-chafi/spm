#include <iostream>
#include <cstdint>

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

    std::cout << "\nOn your system:" << std::endl;
    std::cout << "- Pointer size = " << sizeof(void *) << " bytes (64-bit system)" << std::endl;
    std::cout << "- CompactPayload overhead per item = " << sizeof(CompactPayload) << " bytes" << std::endl;
    std::cout << "- For 100M items: " << (100000000ULL * sizeof(CompactPayload) / 1024 / 1024) << " MB" << std::endl;

    return 0;
}