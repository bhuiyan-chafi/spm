#include <iostream>
#include <vector>
#include <random>
#include <ff/ff.hpp>

using namespace ff;

using task_t = std::pair<float, size_t>;
const size_t minVsize = 512;
const size_t maxVsize = 8192;

// first stage of the pipe
struct Source : ff_node_t<task_t>
{
	// is the constructor of this source struct
	Source(const size_t length) : length(length) {}
	// takes an input type task_t and returns another task_t
	task_t *svc(task_t *)
	{
		auto random01 = []()
		{
			static std::mt19937 generator;
			std::uniform_real_distribution<float> distribution(0, 1);
			return distribution(generator);
		};
		auto random = [](const int &min, const int &max)
		{
			static std::mt19937 generator;
			std::uniform_int_distribution<int> distribution(min, max);
			return distribution(generator);
		};

		for (size_t i = 0; i < length; ++i)
		{
			float x = random01();
			size_t size = random(minVsize, maxVsize);
			// generating the stream withing svc(service) method
			ff_send_out(new task_t(x, size));
		}
		// EOS - End of Stream
		return EOS;
	}

	const size_t length;
};
// second stage of the pipe
struct dotProd : ff_node_t<task_t, float>
{
	float *svc(task_t *task)
	{
		/**
		 * this union here is a trick to save new allocation in the heap.
		 * for the random value generated, we can pass it as a new allocated value new float(r)
		 * but this allocation requires size of a float 32bits at least in the heap, which is expensive for millions of values(large loops)
		 * so, instead of new allocation we will just pass a pointer to that value
		 */
		union
		{
			float r;
			float *ptr;
		} U;
		float x = task->first;
		size_t size = task->second;

		V1.reserve(size);
		V2.reserve(size);
		for (size_t i = 0; i < size; ++i)
		{
			V1.push_back(x * i);
			V2.push_back(x * i / size);
		}

		U.r = dotprod(V1, V2);
		// passing the pointer -> r
		ff_send_out(U.ptr);
		// clearing the vectors because we reserved it
		V1.clear();
		V2.clear();
		delete task;
		return GO_ON;
	}

	float dotprod(std::vector<float> &V1,
				  std::vector<float> &V2)
	{
		float sum = 0.0;
		for (size_t i = 0; i < V1.size(); ++i)
			sum += V1[i] * V2[i];
		return sum;
	}

	std::vector<float> V1;
	std::vector<float> V2;
};
// third stage of the pipe
struct Sink : ff_node_t<float>
{
	float *svc(float *f)
	{
		// same trick but this time the pointer is not used
		union
		{
			float r;
			float *ptr;
		} U;
		U.ptr = f;
		sum += U.r;
		return GO_ON;
	}

	void svc_end() { std::printf("sum= %.4f\n", std::sqrt(sum)); }
	float sum{0.0};
};

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		std::cerr << "use: " << argv[0] << " stream-length\n";
		return -1;
	}
	long length = std::stol(argv[1]);
	if (length <= 0)
	{
		std::cerr << "stream-length should be greater than 0\n";
		return -1;
	}

	Source first(length);
	dotProd second;
	Sink third;

	ff_Pipe pipe(first, second, third);

	// starts the pipeline, if anything is wrong during compilation or run-time it will return -1
	if (pipe.run_and_wait_end() < 0)
	{
		error("running pipe");
		return -1;
	}
	std::cout << "Time: " << pipe.ffTime() << "\n";
	// pipe.ffStats(std::cout);
	return 0;
}
