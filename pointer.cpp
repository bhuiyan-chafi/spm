#include <iostream>
using namespace std;
int main()
{
    int main_value = 10;
    int *pointer = &main_value;
    cout << "Pointer's address: " << &pointer << endl;
    cout << "Pointing address: " << pointer << endl;
    cout << "Current value: " << main_value << endl;

    int array[] = {1, 2, 3, 5};
    int *array_pointer = array;

    cout << "Array address: " << array_pointer << endl;
    return 0;
}
