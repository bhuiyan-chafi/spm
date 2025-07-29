#include <iostream>
#include <cstdint>
#include <vector>
#include <cstdlib>
#include <cmath>
using namespace std;

double unrolled_sum_f2(double *item, u_int64_t size_of_vector)
{
    double sum_0 = 0.0f, sum_1 = 0.0f;

    for (u_int64_t i = 0; i < size_of_vector; i += 2)
    {
        sum_0 += item[i + 0];
        sum_1 += item[i + 1];
    }

    return sum_0 + sum_1;
}

double unrolled_sum_f4(double *item, u_int64_t size_of_vector)
{
    double sum_0 = 0.0f, sum_1 = 0.0f,
           sum_2 = 0.0f, sum_3 = 0.0f;

    for (u_int64_t i = 0; i < size_of_vector; i += 4)
    {
        sum_0 += item[i + 0];
        sum_1 += item[i + 1];
        sum_2 += item[i + 2];
        sum_3 += item[i + 3];
    }

    return sum_0 + sum_1 + sum_2 + sum_3;
}

double unrolled_sum_f8(double *item, u_int64_t size_of_vector)
{
    double sum_0 = 0.0f, sum_1 = 0.0f,
           sum_2 = 0.0f, sum_3 = 0.0f, sum_4 = 0.0f,
           sum_5 = 0.0f, sum_6 = 0.0f, sum_7 = 0.0f;

    for (u_int64_t i = 0; i < size_of_vector; i += 8)
    {
        sum_0 += item[i + 0];
        sum_1 += item[i + 1];
        sum_2 += item[i + 2];
        sum_3 += item[i + 3];
        sum_4 += item[i + 4];
        sum_5 += item[i + 5];
        sum_6 += item[i + 6];
        sum_7 += item[i + 7];
    }

    return sum_0 + sum_1 + sum_2 + sum_3 +
           sum_4 + sum_5 + sum_6 + sum_7;
}

double unrolled_sum_f12(double *item, u_int64_t size_of_vector)
{
    double sum_0 = 0.0f, sum_1 = 0.0f,
           sum_2 = 0.0f, sum_3 = 0.0f, sum_4 = 0.0f,
           sum_5 = 0.0f, sum_6 = 0.0f, sum_7 = 0.0f, sum_8 = 0.0f,
           sum_9 = 0.0f, sum_10 = 0.0f, sum_11 = 0.0f;

    for (u_int64_t i = 0; i < size_of_vector; i += 12)
    {
        sum_0 += item[i + 0];
        sum_1 += item[i + 1];
        sum_2 += item[i + 2];
        sum_3 += item[i + 3];
        sum_4 += item[i + 4];
        sum_5 += item[i + 5];
        sum_6 += item[i + 6];
        sum_7 += item[i + 7];
        sum_8 += item[i + 8];
        sum_9 += item[i + 9];
        sum_10 += item[i + 10];
        sum_11 += item[i + 11];
    }

    return sum_0 + sum_1 + sum_2 + sum_3 +
           sum_4 + sum_5 + sum_6 + sum_7 + sum_8 +
           sum_9 + sum_10 + sum_11;
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
        // check if the size is multiple of 2, 4, or 8 to avoid tailing issues
        if (size_of_vector == 0 || size_of_vector % 2 != 0 && size_of_vector % 4 != 0 && size_of_vector % 8 != 0 || size_of_vector % 12 != 0)
        {
            throw invalid_argument("Size of vector must be a multiple of 2, 4, 8 or 12 and more than 0.");
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    vector<double> vec(size_of_vector);
    double sum_result = 0.0f;
    // initialize the vector with some values
    start = clock();
    for (u_int64_t i = 0; i < size_of_vector; ++i)
    {
        vec[i] = i * 0.1f;
    }
    end = clock();
    cpu_time_used = static_cast<double>(end - start) / CLOCKS_PER_SEC;
    cout << "Time taken to initialize the vector: " << cpu_time_used << " seconds" << endl
         << endl;

    // let's perform the sum using loop unrolling with a factor of 2
    cout << "Starting loop unrolling with factor-2..." << endl;
    start = clock();
    sum_result = unrolled_sum_f2(vec.data(), size_of_vector);
    end = clock();
    cpu_time_used = static_cast<double>(end - start) / CLOCKS_PER_SEC;
    cout << "Time taken to perform unrolled sum: " << cpu_time_used << " seconds" << endl;
    cout << "Unrolled sum of the vector: " << sum_result << endl
         << endl;
    sum_result = 0.0f;

    // with a factor of 4
    cout << "Starting loop unrolling with factor-4..." << endl;
    start = clock();
    sum_result = unrolled_sum_f4(vec.data(), size_of_vector);
    end = clock();
    cpu_time_used = static_cast<double>(end - start) / CLOCKS_PER_SEC;
    cout << "Time taken to perform unrolled sum: " << cpu_time_used << " seconds" << endl;
    cout << "Unrolled sum of the vector: " << sum_result << endl
         << endl;
    sum_result = 0.0f;

    // let's try one additional unrolling with a factor of 8, because stride-8 should give us a better performance by theory
    cout << "Starting loop unrolling with factor-8..." << endl;
    start = clock();
    sum_result = unrolled_sum_f8(vec.data(), size_of_vector);
    end = clock();
    cpu_time_used = static_cast<double>(end - start) / CLOCKS_PER_SEC;
    cout << "Time taken to perform unrolled sum: " << cpu_time_used << " seconds" << endl;
    cout << "Unrolled sum of the vector: " << sum_result << endl
         << endl;
    sum_result = 0.0f;

    // let's try one additional unrolling with a factor of 12 to check if it hurts performance
    cout << "Starting loop unrolling with factor-12..." << endl;
    start = clock();
    sum_result = unrolled_sum_f12(vec.data(), size_of_vector);
    end = clock();
    cpu_time_used = static_cast<double>(end - start) / CLOCKS_PER_SEC;
    cout << "Time taken to perform unrolled sum: " << cpu_time_used << " seconds" << endl;
    cout << "Unrolled sum of the vector: " << sum_result << endl
         << endl;
    // clean up
    vec.clear();
    vec.shrink_to_fit();
    cout << "Vector cleared and memory released." << endl;
    return 0;
}