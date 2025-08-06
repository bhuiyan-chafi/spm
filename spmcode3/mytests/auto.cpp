#include <iostream>
using namespace std;

auto addNumbers(int a, int b)
{
    return a + b;
}

auto multiplyNumbers(double a, double b)
{
    return a * b;
}

auto addStrings(string str1, string str2)
{
    return str1 + str2;
}

int main()
{
    auto variable = addNumbers(5, 10);
    cout << "Sum of 5 and 10: " << variable << endl;
    variable = multiplyNumbers(2.5, 4.0);
    cout << "Product of 2.5 and 4.0: " << variable << endl;
    // auto is not for dynamic typing, but for type deduction, to write cleaner code
    // uncomment the next line to see how it works with strings
    // variable = addStrings("Hello, ", "World!"); //throws an error
    cout << "Concatenation of strings: " << variable << endl;
    return 0;
}