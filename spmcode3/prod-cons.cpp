// The classical Producer-Consumer concurrency problem. 
// This is the version with a buffer of unbounded capacity.
//
//
#include <iostream>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <random>
#include <thread>

int main(int argc, char *argv[]) {
	int nprod = 4;
	int ncons = 3;
	if (argc != 1 && argc != 3) {
		std::printf("use: %s #prod #cons\n", argv[0]);
		return -1;
	}
	if (argc > 1) {
		nprod = std::stol(argv[1]);
		ncons = std::stol(argv[2]);
	}
	
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

	std::mutex mtx;
	std::condition_variable cv; // used to wait if the buffer is empty
	std::deque<int64_t> dataq;

	auto random = [](const int &min, const int &max) {
		// thread_local storage: 
		// (https://en.cppreference.com/w/cpp/language/storage_duration)
		// It allows to create a separate per-thread storage for a variable 
		// Here used to store a dedicated random number generator (with a
		// dedicated seed) for each thread.
		thread_local std::mt19937   
			generator(std::hash<std::thread::id>()(std::this_thread::get_id()));
		std::uniform_int_distribution<int> distribution(min,max);
		return distribution(generator);
	};		
	// producer function
	auto producer = [&](const int64_t ntasks, int id) {	   
		for(int64_t i=0; i<ntasks; ++i) {
			// the queue is unbounded, there is no condition of buffer full
			{
				// RAII-style mutex wrapper
				std::lock_guard<std::mutex> lock(mtx);
				dataq.push_back(i);
			}
			cv.notify_one();
			// do something
			std::this_thread::sleep_for(std::chrono::milliseconds(random(0,100))); 
		}
		
		std::printf("Producer%d produced %ld\n",id, ntasks);
	};
	// consumer function
	auto consumer = [&](int id) {
		uint64_t ntasks{0};
		// RAII-style mutex wrapper
		// here the lock acquisition is deferred
		std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
		for(;;) {
			lock.lock();
			while(dataq.empty()) {
				cv.wait(lock);  // atomically release the lock and wait
			}
			// Alternative to while: cv.wait(lock, predicate);
			// the predicate returns true when the thread can go on
			//cv.wait(lock, [&]() { return !dataq.empty(); });
			
			auto data = dataq.front();
			dataq.pop_front();
			lock.unlock();
			if (data == -1) {
				break;
			}
			++ntasks;
			// do something
			std::this_thread::sleep_for(std::chrono::milliseconds(random(0,100))); 
		}
		std::printf("Consumer%d consumed %lu tasks\n",id, ntasks);
	};
	
	for (int i = 0; i < ncons; ++i)
        consumers.emplace_back(consumer, i);	
    for (int i = 0; i < nprod; ++i)
        producers.emplace_back(producer, 100, i);

    // wait all producers
	for (auto& thread : producers) thread.join();
	// stopping the consumers inserting a "special value" into the queue
	{
		std::unique_lock<std::mutex> lock(mtx);
		for (int64_t i = 0; i < ncons; ++i)
			dataq.push_back(-1);
		cv.notify_all();
	}
	// wait for all consumers
	for (auto& thread : consumers) thread.join();


    return 0;
}
