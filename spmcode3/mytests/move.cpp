#include <iostream>
#include <vector>
using namespace std;

int main()
{
    vector<int> buffer = {1, 2, 3, 4, 5};
    vector<int> newBuffer;
    newBuffer = move(buffer); // Move the contents of buffer to newBuffer
    if (buffer.empty())
    {
        cout << "Buffer is empty after move. But it left a memory address: " << &buffer << endl;
    }
    else
    {
        cout << "Buffer still has elements after move." << endl;
    }
    for (auto buf : buffer) // this will print nothing because buffer is empty = false
    {
        cout << buf << endl;
        cout << "Memory locations of the old buffer: " << &buf << endl;
    }
    return 0;
}