#include <iostream>
#include <vector>
using namespace std;

int main()
{
    vector<string> names{"Audi", "Mercedes", "Ferrari", "Ford"}; // a simple array(1-D)
    vector<double> prices{100, 101, 102, 103};                   // a simple array(1-D)
    vector<string> buyers{"CHAFI", "RIA", "ARHAM", "ARSHI"};     // a simple array(1-D)
    vector<vector<string>> sales(10, vector<string>(10));        // a 2-D array where the first parameter is the size of the outer vector and the second is the size of the inner vector.
    // bids
    sales[0][0] = "Arham";
    sales[0][1] = "90";
    // has 10 slots from 0 to 9
    sales[0][9] = "102";
    // starting of the second row
    sales[1][0] = "Arshi";
    sales[1][1] = "90";
    // also has the position from 0 to 9
    sales[1][9] = "103";
    // can get upto position [9]][9]
}
