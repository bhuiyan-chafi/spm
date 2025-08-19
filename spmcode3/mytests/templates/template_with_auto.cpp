#include <iostream>
using namespace std;

template <typename T>
T square(T x)
{
    return x * x;
}

int main()
{
    auto a = square(5);    // T deduced as int → a is int
    auto b = square(3.14); // T deduced as double → b is double

    cout << a << " " << b << endl;
}
