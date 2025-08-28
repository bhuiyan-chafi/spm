/**
 * main author: Professor Massiomo Torquati.
 * Departmento di Informatica
 * Universita di Pisa, Pisa, Italy
 * sub-author: ASM Chafiullah Bhuiyan
 * Departmento di Informatica, Curriculumn: Informatica e Networking
 * ============================= About ================================
 * First assignemnt of the Course: Parallel and Distributed Systems. The main intention is to know
 * about the intrinsic of vectorization. The code below is well documented, but the documentation focuses
 * on knowledge rather than technical aspects.
 */
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <limits>
#include <immintrin.h>
#include "../include/hpc_helpers.hpp"
#include "../include/avx_mathfun.h"

/**
 * ============== Additional Functions for Debugging ==================
 * Function Names: check__m256_vmax_values,values_in_tail,check__m256_accumulated_values
 */
void check__m256_accumulated_values(__m256 accumulated_values)
{
	// alignas because the lanes[0~7] are ordered inside __m256
	alignas(32) float temp_arr_store[8];
	// load and store the __m256 inside temp_arr_store
	_mm256_storeu_ps(temp_arr_store, accumulated_values);
	printf("============ Printing the values of `__m256` storage `accumulated_values` ============\n");
	for (size_t i = 0; i < 8; i++)
	{
		printf("Lane[%ld] : %f \n", i, temp_arr_store[i]);
	}
	std::cout << std::endl;
}

void check__m256_vmax_values(__m256 vmax)
{
	// alignas because the lanes[0~7] are ordered inside __m256
	alignas(32) float temp_arr_store[8];
	// load and store the __m256 inside temp_arr_store
	_mm256_storeu_ps(temp_arr_store, vmax);
	printf("============ Printing the values of `__m256` storage `vmax` ============\n");
	for (size_t i = 0; i < 8; i++)
	{
		printf("Lane[%ld] : %f \n", i, temp_arr_store[i]);
	}
	std::cout << std::endl;
}

void values_in_tail(const float *__restrict__ input, size_t index, size_t K)
{
	printf("============ Printing the tail values ============\n");
	for (; index < K; index++)
	{
		printf("Position[%ld] : %f \n", index, input[index]);
	}
	std::cout << std::endl;
}

void benchmark_output(const std::vector<float> &output)
{
	float output_sum = 0.0f;
	for (auto &item : output)
	{
		output_sum += item;
	}
	std::cout << "Final Benchmark Sum is: " << output_sum << std::endl;
}

/**
 * ================== the main softmax_avx function ======================
 */
void softmax_avx(const float *__restrict__ input, float *__restrict__ output, size_t K)
{
	TIMERSTART(max_finder_loop);
	// first we will create the chunks and calculate how many items are remaining(of course < 8)
	const size_t highest_chunk_value = K & ~size_t(7); // largest multiple of 8
	const size_t tail = K - highest_chunk_value;
	// printf("Highest Chunk Value: %ld and Remaining Tail: %ld \n", highest_chunk_value, tail);
	/**
	 * operation on the aligned part from 0~highest_chunk_value to find the max
	 * then we will perform the same operation on the tail(which is < 8 of course)
	 * declaring one avx2 which is 256bits, it has 8 lanes 0~7. So, yes it is a vector of course
	 * 8 because 4byte float = 32bits, 256/32=8
	 */
	__m256 vmax = _mm256_set1_ps(-std::numeric_limits<float>::infinity());
	// after this operation this is the scenario inside `vmax` = [-inf,.......,-inf](-inf is broadcasted to all 8 lanes)
	// now we will run the loop to find the max within the chunk range 0~highest_chunk_value
	size_t index = 0; // declaring it globally so that we can use it for the tail
	for (; index < highest_chunk_value; index += 8)
	{
		// unaligned load[0~7,8~15,16~23,....,x~highest_chunk_value]
		__m256 v = _mm256_loadu_ps(input + index);
		// for each 8-stride load, sending the max value to vmax vector
		vmax = _mm256_max_ps(vmax, v);
		// after finishing the loop each lane of the `vmax` will hold the max value of the received elements
		// the numbers are positions of the elements
		// lane0: max(0,8,16,......,xN)
		// lane1: max(1,9,17,....,xN+1)
		// lane2: max(2,10,18)
		//...
		//...
		// lane7: max(7,15,23,.....,xN+7)
	}
	// printf("Current Value of the `index`: %ld \n", index);
	/**
	 * Let's have a look how our `vmax` register storage looks like. Since there is no direct way to run a loop on `__m256` type, we will create a temporary array and store the values there.
	 */
	// check__m256_vmax_values(vmax);
	/**
	 * Now we have the tail remaining but the inside `vmax` is not a scalar value but a 8 lane vector
	 * We have to make it scalar and run another loop to find out the max among maxes(lane 0~7)
	 * Scalar because just 8 values to iterate, not big loop like the chunk
	 */
	// declare one aligned temporary float array
	// aligned because lanes are organized 0~7
	alignas(32) float temp_arr_vmax[8];
	// load and store the __m256 vmax
	_mm256_storeu_ps(temp_arr_vmax, vmax);
	// one scalar variable to keep the max
	float final_max_value = temp_arr_vmax[0];
	for (int i = 1; i < 8; i++)
	{
		final_max_value = std::max(final_max_value, temp_arr_vmax[i]);
	}
	// printf("Max value from `vmax` vector: %f \n", final_max_value);
	/**
	 * Now we have one scalar max and the remaining tail
	 */
	// printf("================= Going for the tail part ====================\n");
	// printf("Starting from index: %ld till the end of Size: %ld\n", index, K);
	std::cout << std::endl;
	// values_in_tail(input, index, K);
	for (; index < K; index++)
	{
		final_max_value = std::max(final_max_value, input[index]);
	}
	// printf("Final max value from the input: %f \n", final_max_value);
	TIMERSTOP(max_finder_loop);
	/**
	 * The second phase is to perform the exp on the input and finding the total max
	 * the scalar operation was: output[i] = std::exp(input[index] - final_max_value);
	 * and then: sum += output[i];
	 */
	// take an avx2 lane to broadcast the `final_max_value` because we have to perform
	// input[index]-final_max_value
	TIMERSTART(reduction_loop);
	const __m256 final_max_avx = _mm256_set1_ps(final_max_value); // now we have the same value broadcasted to 8 lanes of the `avx` vector `final_max_avx`
	// taking one accumulator avx2 to calculate the total sum
	__m256 accumulator_avx = _mm256_set1_ps(0.0f); // because the final sum is a float scalar
	// grounding the previous index
	index = 0;
	for (; index < highest_chunk_value; index += 8)
	{
		// create a new avx2 and load 8-stride
		__m256 current_stride = _mm256_loadu_ps(input + index);
		// perform the subtraction
		__m256 subtracted_stride = _mm256_sub_ps(current_stride, final_max_avx); // so we are subtracting 8 lanes of the final_max_avx from 8 lanes of the current_stride, and we are doing it at once
		// now time to perform the `exp`
		__m256 exp_stride = exp256_ps(subtracted_stride);
		// storing the value in the output
		_mm256_storeu_ps(output + index, exp_stride);
		// accumulate the value
		accumulator_avx = _mm256_add_ps(accumulator_avx, exp_stride);
	}
	// let's check how our accumulator looks like
	// check__m256_accumulated_values(accumulator_avx);
	// now we have a 8-stride avx values inside accumulator that we have to sum up
	//  we will use the same `temp_arr_vmax` to perform the store
	_mm256_storeu_ps(temp_arr_vmax, accumulator_avx);
	float sum = 0.0f;
	// perform the scalar sum which is very cheap in computation
	for (int i = 0; i < 8; i++)
	{
		sum += temp_arr_vmax[i];
	}
	// print out the sum without the tail
	// std::cout << "The sum without tail: " << sum << std::endl;
	// add the tail part which is also a small scalar loop
	for (; index < K; index++)
	{
		// here we are performing scalar operation, keep that in mind
		const float temp_exp = std::exp(input[index] - final_max_value);
		output[index] = temp_exp;
		sum += temp_exp;
	}
	// print out the final sum
	// std::cout << "Final Sum: " << sum << std::endl;
	TIMERSTOP(reduction_loop);
	/**
	 * Now we will proceed with the normalization
	 * output[i] /= sum in scalar
	 * divide the output in chunks and then perform the division on the elements but remember that
	 * a multiplication is faster than a division
	 * 5/10 = 5*1/10 = 5*0.1 = 0.5
	 */
	// let's inverse the sum
	TIMERSTART(normalizer);
	float sum_inverse = 1.0f / sum;
	// printf("Value of the Sum: %f and Sum Inverse: %.9f\n", sum, sum_inverse);
	// broadcast the value into an avx2 new vector. Since, I don't have to clear them manually! I can create another avx2 for nomenclature(naming convention)
	const __m256 final_sum_avx = _mm256_set1_ps(sum_inverse);
	index = 0;
	for (; index < highest_chunk_value; index += 8)
	{
		// create a new avx2 and load the 8-stride
		//  output points the first element and index = 0
		//  this loads the first 8 elements from the `output` vector
		__m256 current_stride = _mm256_loadu_ps(output + index);
		// now we will perform the multiplication(the actual division in the plain version)
		__m256 multiplied_stride = _mm256_mul_ps(current_stride, final_sum_avx);
		// store the value in output(basically we are overwriting the values inside output)
		_mm256_storeu_ps(output + index, multiplied_stride);
	}
	// Going for the tail part to normalize
	for (; index < K; index++)
	{
		output[index] *= sum_inverse;
	}
	TIMERSTOP(normalizer);
}

std::vector<float> generate_random_input(size_t K, float min = -1.0f, float max = 1.0f)
{
	std::vector<float> input(K);
	// std::random_device rd;
	// std::mt19937 gen(rd());
	std::mt19937 gen(5489); // fixed seed for reproducible results
	std::uniform_real_distribution<float> dis(min, max);
	for (size_t i = 0; i < K; ++i)
	{
		input[i] = dis(gen);
	}
	return input;
}

void printResult(std::vector<float> &v, size_t K)
{
	for (size_t i = 0; i < K; ++i)
	{
		std::fprintf(stderr, "%f\n", v[i]);
	}
}

int main(int argc, char *argv[])
{
	if (argc == 1)
	{
		std::printf("use: %s K [1]\n", argv[0]);
		return 0;
	}
	size_t K = 0;
	if (argc >= 2)
	{
		K = std::stol(argv[1]);
		if (K < 8)
		{
			std::cout << "The main intention is to check the performance for larger input size which is at least > 8" << std::endl;
			return EXIT_FAILURE;
		}
	}
	bool print = false;
	if (argc == 3)
	{
		print = true;
	}
	std::vector<float> input = generate_random_input(K);
	std::vector<float> output(K);

	TIMERSTART(softime_avx);
	softmax_avx(input.data(), output.data(), K);
	TIMERSTOP(softime_avx);
	/**
	 * Let's perform a scalar sum to check if sum(output) = 1
	 */
	// benchmark_output(output);
	// print the results on the standard output
	if (print)
	{
		printResult(output, K);
	}
}
