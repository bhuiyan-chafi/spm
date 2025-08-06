#include <cmath>
#include <iostream>

double speedup(int k, int q, double gamma)
{
    double num = gamma * (std::pow(2.0, k) - 1.0);
    double den = 2.0 * q + gamma * (std::pow(2.0, k - q) - 1.0 + q);
    return num / den;
}

int main()
{
    int k = 10;         // global array = 2^k
    int q = 4;          // number of PEs = 2^q
    double alpha = 1.0; // 1s per add
    double beta = 3.0;  // 3s per message
    double gamma = alpha / beta;

    double S = speedup(k, q, gamma);
    std::cout << "gamma = " << gamma << "\n";
    std::cout << "Speedup on p=2^" << q << " = " << S << "\n";
    std::cout << "Efficiency = " << (S / std::pow(2.0, q)) * 100.0
              << "%\n";
}
