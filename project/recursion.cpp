#include <iostream>
#include <spdlog/spdlog.h>
using namespace std;

int recursiveSum(int limit)
{
    if (limit > 0)
    {
        spdlog::info("Summing up limit:{} + limit-1:{}", limit, limit - 1);
        /**
         * each of these calls will be placed on the call stack until the base case is reached
         */
        return limit + recursiveSum(limit - 1);
    }
    else
    {
        spdlog::warn("Base case reached, returning 0");
        return 0;
    }
}

int main(int argc, char *argv[])
{
    if (argc > 2)
    {
        spdlog::error("There should not be more than 2 arguments");
        return EXIT_FAILURE;
    }

    int limit = argv[1] ? stoi(argv[1]) : 10; // default limit is 10
    int result = recursiveSum(limit);
    cout << "The sum from 1 to " << limit << " is: " << result << endl;
    return 0;
}