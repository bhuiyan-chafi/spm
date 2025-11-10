#include <iostream>
#include <cstdint>

int main()
{
    std::cout << "=== Size Comparison ===" << std::endl;
    std::cout << "uint8_t (the type):        " << sizeof(uint8_t) << " byte" << std::endl;
    std::cout << "uint8_t* (pointer to it):  " << sizeof(uint8_t *) << " bytes" << std::endl;
    std::cout << std::endl;

    std::cout << "Other examples:" << std::endl;
    std::cout << "int:                       " << sizeof(int) << " bytes" << std::endl;
    std::cout << "int*:                      " << sizeof(int *) << " bytes" << std::endl;
    std::cout << "double:                    " << sizeof(double) << " bytes" << std::endl;
    std::cout << "double*:                   " << sizeof(double *) << " bytes" << std::endl;
    std::cout << std::endl;

    std::cout << "Key insight: ALL pointers are " << sizeof(void *) << " bytes on 64-bit systems!" << std::endl;
    std::cout << "(They just store memory addresses)" << std::endl;

    return 0;
}