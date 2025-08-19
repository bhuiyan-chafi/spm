#include <iostream>
#include <cstdint>
using namespace std;

uint16_t addNumbers(uint16_t a, uint16_t b) { return a + b; }
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cerr << "Expecting exactly 3 paramemters[program_name, int_1 and int_2]" << endl;
        return EXIT_FAILURE;
    }
    // look here for the use of 'auto', this result is not any static type but a lambda function call. You cannot specify it's type before compiling. When you will compile the program it detects the return type and deduces it.
    // also here we didn't send the argv as &argv(by reference) because we are not changing the variable. If we take another variable and want to assign the result in it, we need to pass the the result container as a reference.
    auto result = [argv]()
    {
        return addNumbers(
            static_cast<uint16_t>(std::stoi(argv[1])),
            static_cast<uint16_t>(std::stoi(argv[2])));
    };
    cout << "Result: " << result() << endl;
    return 0;
}