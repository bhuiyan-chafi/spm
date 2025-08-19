#include <iostream>
#include <cstdint>
using namespace std;

int main()
{
    auto callLambda = []()
    {
        cout << "Hello, World!" << endl;
    };
    callLambda();

    // usabel
    uint16_t a = 10;
    uint16_t b = 20;

    auto sum = [&a, &b]()
    {
        return a + b;
    };
    cout << "Sum: " << sum() << endl;

    // parameterized lambda with return type
    // even though we know the return type we cannot specify it in the lambda capture, because it is not a variable
    auto multiply = [](uint16_t x, uint16_t y) -> uint16_t
    {
        return x * y;
    };
    cout << "Multiply: " << multiply(a, b) << endl;
    return 0;
}