#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <variant>
#include <threadPool.hpp>


int main() {
    // Example thread pool with 3 workers
    ThreadPool TP(3);

    // Prepare a single vector of futures that can hold either uint64_t or std::string
    using ResultVariant = std::variant<uint64_t, std::string>;
    std::vector<std::future<ResultVariant>> futures;

    auto square = [](uint64_t x) {
        return ResultVariant{x*x}; 
    };
	auto toupper = [](const std::string& lower) {
		std::string result(lower.size(),' ');
		std::transform(lower.begin(), lower.end(), result.begin(),
					   [](unsigned char c){ return std::toupper(c); });
		return ResultVariant{result};
	};
	
	futures.emplace_back(TP.enqueue(square, 12));
	futures.emplace_back(TP.enqueue(square, 31));
	futures.emplace_back(TP.enqueue(toupper, "hello"));
	futures.emplace_back(TP.enqueue(toupper, "world"));

    for (auto& f : futures) {
        // get() yields a std::variant<uint64_t, std::string>
        auto val = f.get();
#if 0
		// print the values using std::visit that performs "pattern matching" on the types
		std::visit([](const auto& value) { std::cout << value << " "; }, val);
#else
		// We can manually check which type is currently held
        if (std::holds_alternative<uint64_t>(val)) {
            std::cout << std::get<uint64_t>(val) << " ";
        } else {
            std::cout << std::get<std::string>(val) << " ";
        }
#endif		
    }
	std::cout << "\n";
}
