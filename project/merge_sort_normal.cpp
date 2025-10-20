#include <iostream>
#include <vector>
#include <spdlog/spdlog.h>

using namespace std;

// Function to merge two halves
void merge(vector<int> &arr, int left, int mid, int right)
{
    spdlog::info("Entered merging function with left={}, mid={}, right={}", left, mid, right);
    int n1 = mid - left + 1; // size of left subarray
    int n2 = right - mid;    // size of right subarray
    spdlog::info("Merging subarrays of sizes {} and {}", n1, n2);
    // Temporary arrays
    vector<int> L(n1), R(n2);

    // Copy data to temporary arrays
    for (int i = 0; i < n1; i++)
        L[i] = arr[left + i];
    spdlog::info("Left subarray values: {}", fmt::join(L, ", "));
    for (int i = 0; i < n2; i++)
        R[i] = arr[mid + 1 + i];
    spdlog::info("Right subarray values: {}", fmt::join(R, ", "));
    // Merge the temp arrays back into arr[left..right]
    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2)
    {
        if (L[i] <= R[j])
            arr[k++] = L[i++];
        else
            arr[k++] = R[j++];
    }

    // Copy remaining elements, if any
    while (i < n1)
        arr[k++] = L[i++];
    while (j < n2)
        arr[k++] = R[j++];
    spdlog::info("Merged array segment[]: {}", fmt::join(vector<int>(arr.begin() + left, arr.begin() + right + 1), ", "));
}

// Recursive function to sort array
void mergeSort(std::vector<int> &arr, int left, int right, int depth = 0)
{
    std::string indent(depth * 2, ' ');
    spdlog::info("{}entered mergeSort([left:{}, right:{}])", indent, left, right);

    if (left >= right)
    {
        spdlog::info("{}base case [left:{} == right:{}]", indent, left, right);
        return;
    }

    int mid = left + (right - left) / 2;
    spdlog::info("{}split at mid = {}", indent, mid);

    mergeSort(arr, left, mid, depth + 1);
    spdlog::info("{}left done: [left:{}, mid:{}, right:{}]", indent, left, mid, right);

    mergeSort(arr, mid + 1, right, depth + 1);
    spdlog::info("{}right done: [mid+1:{}, right:{}]", indent, mid + 1, right);

    spdlog::info("{}merge([left:{},mid:{}) + [mid+1:{},right:{}])", indent, left, mid, mid + 1, right);
    merge(arr, left, mid, right);
    spdlog::info("{}merged: [left:{}, right:{}]", indent, left, right);
    spdlog::info("Finished one call stack ...........................................");
}

int main()
{
    vector<int> arr = {5, 7, 1, 4, 2, 3, 6, 0};
    spdlog::info("Unsorted array {}: ", fmt::join(arr, ", "));
    spdlog::info("Starting merge sort...");
    mergeSort(arr, 0, arr.size() - 1);
    spdlog::info("Sorted array: {}", fmt::join(arr, ", "));
    return 0;
}