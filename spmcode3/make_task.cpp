#include <cstdint>
#include <iostream>
#include <future>
#include <functional>
#include <thread>

template<
	typename Func,    // <-- type of the func
	typename ... Args,// <-- arguments type arg0, arg1, ...
	typename Rtrn=typename std::result_of<Func(Args...)>::type
	>                 // ^-- type of the return value func(args)     
auto make_task(
	  Func && func,
	  Args && ...args) -> std::packaged_task<Rtrn(void)> {

	// basically it builds a callable object aux
	// (an auxilliary function aux(void)) without arguments
	// and returning func(arg0,arg1,...)
	auto aux = std::bind(std::forward<Func>(func),
						 std::forward<Args>(args)...);

	// create a task wrapping the auxilliary function:
	// task() executes aux(void) := func(arg0,arg1,...)
	auto task = std::packaged_task<Rtrn(void)>(aux);
	
	// the return value of aux(void) is assigned to a
	// future object accessible via task.get_future()
	return task;
}

// function
bool comp(float value, int64_t threshold) {
	return value <threshold;
}

// a functor
struct AFunctor {
	void operator()(const std::string& s) { std::cout << s << "\n"; }
};


int main(int argc, char * argv[]) {
	using namespace std::chrono_literals;

	auto task = make_task(comp, 10.3, 20);
	
	auto future = task.get_future();
	
	// spawn a thread and detach it
	std::thread thread(std::move(task));
	thread.detach();

	std::cout << future.get() << "\n";
	// cannot join the thread, it is detached!

#if 1
	// task created from a lambda
	auto task2 = make_task(
						   [&](auto s1, auto s2) {
							   std::cout << s1 << s2 << "\n";
						   },
						   "Hello,", " World!\n");
	auto future2 = task2.get_future();
	
	std::thread thread2(std::move(task2));
	thread2.join();

	auto task3 = make_task(AFunctor{}, "Ciao");
	auto future3 = task3.get_future();
	
	std::thread thread3(std::move(task3));
	thread3.detach();
	future3.get();
#endif	
}
