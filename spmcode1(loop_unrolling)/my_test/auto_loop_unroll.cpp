#include <iostream>
#include <cstdint>
#include <vector>
#include <cstdlib>
#include <cmath>
using namespace std;

float linear_sum(float *item, u_int64_t size_of_vector)
{
    float sum = 0.0f;
    for (u_int64_t i = 0; i < size_of_vector; ++i)
    {
        sum += item[i];
    }
    return sum;
}

int main(int argc, char *argv[])
{
    clock_t start, end;
    double cpu_time_used;
    // make sure you have put two arguments: program name and size of the vector
    if (argc != 2)
    {
        cerr << "Usage: " << argv[0] << " <size_of_vector>" << endl;
        return EXIT_FAILURE;
    }
    // initialize the size first with a default value
    u_int64_t size_of_vector;
    // make sure the size you passed is a valid number
    try
    {
        size_of_vector = stoull(argv[1]);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    vector<float> vec(size_of_vector);
    cout << "Size of the vector: " << size_of_vector << endl;
    // initialize the vector with some values
    start = clock();
    for (u_int64_t i = 0; i < size_of_vector; ++i)
    {
        vec[i] = i * 0.01f; // Example initialization
    }
    end = clock();
    cpu_time_used = static_cast<double>(end - start) / CLOCKS_PER_SEC;
    cout << "Time taken to initialize the vector: " << cpu_time_used << " seconds" << endl;

    // let's perform linear sum on the vector
    cout << "Starting linear sum on the vector..." << endl;
    start = clock();
    float sum_result = linear_sum(vec.data(), size_of_vector);
    end = clock();
    cpu_time_used = static_cast<double>(end - start) / CLOCKS_PER_SEC;
    cout << "Time taken to perform linear sum: " << cpu_time_used << " seconds" << endl;
    cout << "Sum of the vector: " << sum_result << endl;

    // clean up
    vec.clear();
    vec.shrink_to_fit();
    cout << "Vector cleared and memory released." << endl;
    return 0;
}