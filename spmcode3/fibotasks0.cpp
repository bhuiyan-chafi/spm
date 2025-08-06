#include <iostream> // std::cout
#include <cstdint>  // uint64_t
#include <vector>   // std::vector
#include <thread>   // std::thread
#include <future>   // std::future


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
		// create one task
		std::packaged_task<uint64_t(uint64_t)> task(fibo);
		// store the future
		results.emplace_back(task.get_future());
		// spawn a thread to execute the task
		threads.emplace_back(std::move(task), id);
	}
    for (auto& result: results) std::cout << result.get() << std::endl;		
    for (auto& thread: threads) thread.join();
}


