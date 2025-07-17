#include <iostream>
using namespace std;

int main()
{
    int main_variable = 10;
    int &reference = main_variable;
    cout << "Current value: " << main_variable << endl;
    reference = 20;
    cout << "New value changing by the reference: " << main_variable << endl;
}