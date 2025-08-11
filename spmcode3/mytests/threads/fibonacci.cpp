#include <iostream>
#include <cstdint>
using namespace std;
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Expecting exactly 2 paramemters[program_name and one integer]" << endl;
        return EXIT_FAILURE;
    }
    const uint16_t limit = static_cast<uint16_t>(stoi(argv[1]));
    uint16_t a = 0, b = 1;
    cout << "Current Fibonacci Number: " << a << endl;
    cout << "Current Fibonacci Number: " << b << endl;
    for (uint16_t i = 0; i < limit; i++)
    {
        const uint16_t current_number = a + b;
        a = b;
        b = current_number;
        cout << "Current Fibonacci Number: " << current_number << endl;
    }

    return 0;
}