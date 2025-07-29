#include <iostream>
using namespace std;

int main()
{
    int variable = 10;
    int *pointer = &variable;  // Pointer to variable
    int &reference = variable; // Reference to variable
    int reference = *pointer;  // Assigning value through pointer

    cout << "Value of variable: " << variable << endl;
    cout << "Value using pointer: " << *pointer << endl;
    cout << "Value using reference: " << reference << endl;
    cout << "Address of variable: " << &variable << endl;
    cout << "Address using pointer: " << pointer << endl;
    cout << "Address using reference: " << &reference << endl;
    return 0;
}