// The classical Producer-Consumer concurrency problem. 
// This is the version with a buffer of bounded capacity.
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
	size_t buffer_capacity=10;
	if (argc != 1 && argc != 4) {
		std::printf("use: %s #prod #cons #buf-capacity\n", argv[0]);
		return -1;
	}
	if (argc > 1) {
		nprod = std::stol(argv[1]);
		ncons = std::stol(argv[2]);
		buffer_capacity=std::stol(argv[3]);
	}
	
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

	std::mutex mtx;
	std::condition_variable cvempty; // buffer empty condition
	std::condition_variable cvfull;  // buffer full condition
	std::deque<int64_t> dataq;

	auto random = [](const int &min, const int &max) {
		// better to have different per-thread seeds....
		thread_local std::mt19937   
			generator(std::hash<std::thread::id>()(std::this_thread::get_id()));
		std::uniform_int_distribution<int> distribution(min,max);
		return distribution(generator);
	};		
	// producer function
	auto producer = [&](const int64_t ntasks, int id) {	   
		std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
		for(int64_t i=0; i<ntasks; ++i) {
			lock.lock();
			cvfull.wait(lock, [&]() { return dataq.size()<buffer_capacity; });
			dataq.push_back(i);
			lock.unlock();			
			cvempty.notify_one();

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
				cvempty.wait(lock);  // atomically release the lock and wait
			}
			auto data = dataq.front();
			dataq.pop_front();
			lock.unlock();
			if (data == -1) {
				break;
			}
			cvfull.notify_one();
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
		cvempty.notify_all();
	}
	// wait for all consumers
	for (auto& thread : consumers) thread.join();


    return 0;
}
