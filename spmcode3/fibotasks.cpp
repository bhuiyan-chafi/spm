#include <iostream> // std::cout
#include <cstdint>  // uint64_t
#include <vector>   // std::vector
#include <thread>   // std::thread
#include <future>   // std::future

#include "taskfactory.hpp"


// traditional signature of fibo
uint64_t fibo(uint64_t n) {
    uint64_t a_0 = 0, a_1 = 1;
    for (uint64_t index = 0; index < n; index++) {
        const uint64_t tmp = a_0; a_0 = a_1; a_1 += tmp;
	}
    return a_0; 
}

// this runs in the master thread
int main(int argc, char * argv[]) {
    const uint64_t num_threads = 32;

    std::vector<std::thread> threads;
    // allocate num_threads many result values
    std::vector<std::future<uint64_t>> results;

	// create tasks, store futures and spawn threads
    for (uint64_t id = 0; id < num_threads; id++) {
		auto task = make_task(fibo, id);
		results.emplace_back(task.get_future());
		threads.emplace_back(std::move(task));
	}
    for (auto& result: results) std::cout << result.get() << std::endl;		
    for (auto& thread: threads) thread.join();
}
