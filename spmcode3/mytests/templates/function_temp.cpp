#include <iostream>
using namespace std;
#include "../../../utils/microtimer.h" // include the microtimer header

template <typename T>
T add(T a, T b)
{
    return a + b;
}

template <typename T, typename U>
auto addWithDifferentType(T a, U b)
{
    return a + b;
}

int main()
{
    auto result = add(5, 10);
    // result = addWithDifferentType(5.5, 10); //returns an integer since the first assignment is an integer
    auto result2 = addWithDifferentType(5, 10.5); // does return a double since double + int is a double
    cout << "Result: " << result << endl;
    cout << "Result2: " << result2 << endl;
    return 0;
}