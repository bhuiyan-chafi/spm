#include <iostream>
using namespace std;

int main()
{
    int main_variable = 10;
    int &reference = main_variable;
    cout << "Address of main_variable: " << &main_variable << endl;
    cout << "Address of reference: " << &reference << endl;
    cout << "Current value: " << main_variable << endl;
    reference = 20;
    cout << "New value changing by the reference: " << main_variable << endl;
    // lets change the value using the main_variable
    main_variable = 30;
    cout << "New value changing by the main_variable: " << reference << endl;
    return 0;
}
