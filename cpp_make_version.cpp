#include <iostream>
#include <optional>
int main()
{
    std::optional<int> x = 42;
    if (x)
        std::cout << "Optional value: " << *x << '\n';
    std::cout << "C++ version: " << __cplusplus << std::endl;
    return 0;
}
