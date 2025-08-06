#include <iostream>
#include <vector>

#include "../../../utils/microtimer.h" // include the microtimer header

using namespace std;

template <typename T>
struct MaxValue
{
    T maxValue;
    MaxValue(T value) : maxValue(value) {}; // is the equivalent of a constructor
    bool checkCurrentValue(T value)
    {
        if (value > maxValue)
        {
            maxValue = value;
            return true;
        }
        return false;
    }
};

template <typename X>
struct EvenOddDetector
{
    X value;
    EvenOddDetector(X val) : value(val) {};
    bool operator()(int currentValue)
    {
        if (currentValue > 0 && currentValue % 2 == 0)
        {
            return true;
        }
        return false;
    }
};

int main()
{
    // taking a dynamic array of integers
    vector<int> values = {10, 5, 15, -1, 20, 18};
    // initialize the `struct` with the first value
    MaxValue tracker(values.front());
    for (const int &value : values)
    {
        cout << "Going out with value: " << value << endl;
        if (tracker.checkCurrentValue(value))
        {
            cout << "Current value that has been checked: " << tracker.maxValue << endl;
            cout << "New max value found: " << tracker.maxValue << endl;
        }
    }
    cout << "------------------------" << endl;
    // Let's do the same uisng a functor
    EvenOddDetector detector(values.front());
    for (const int &value : values)
    {
        cout << "Going out with value: " << value << endl;
        if (detector(value))
        {
            cout << "Current value that has been checked: " << value << endl;
            cout << "New even value found: " << value << endl;
        }
    }
    return 0;
}
