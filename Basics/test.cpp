#include <vector>
#include <iostream>

int main()
{
    // this is to check data type size overflow.
    std::cout << "Max Size: ";
    std::cout << std::vector<double>().max_size();
    return 0;
}