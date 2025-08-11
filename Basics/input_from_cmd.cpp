#include <iostream>
using namespace std;

int main(int argc, char *argv[])
{
    // we expect at least two arguments: program name and a number
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " <number>" << argv[1] << endl;
        return EXIT_FAILURE;
    }
    cout << "Program Name: " << argv[0] << " & You entered: " << argv[1] << endl;
    return 0;
}