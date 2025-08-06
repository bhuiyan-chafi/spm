#include <iostream>
#include <vector>
#include <utility>
#include <string>
#include <algorithm>
#include <functional>


// ---------------------------------------------------
// Functor: MaxTracker
// ---------------------------------------------------
template <typename T>
struct MaxTracker {
    MaxTracker(const T& init) : current_max(init) {


	}

    bool operator()(const T& val) {
        if (val > current_max) {
            current_max = val;
            return true;
        }
        return false;
    }
    T current_max;
};

// ---------------------------------------------------
// Class with Move Semantics: Resource
// ---------------------------------------------------
template <typename T>
class Resource {
public:
    // Constructor from a vector of T.
    Resource(const std::vector<T>& data) : data_(data) {
        std::cout << "Resource constructed\n";
    }

    // Move constructor
    Resource(Resource&& other) noexcept
        : data_(std::move(other.data_)) {
        std::cout << "Resource moved\n";
    }

    // Move assignment operator
    Resource& operator=(Resource&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            std::cout << "Resource move-assigned\n";
        }
        return *this;
    }

    // Copy constructor and assignment for completeness
    Resource(const Resource& other) = default;
    Resource& operator=(const Resource& other) = default;

    void print(const std::string& name) const {
        std::printf("Resource data of %s: ", name.c_str());
        for (const auto& item : data_) {
            std::cout << item << " ";
        }
        std::cout << "\n";
    }

private:
    std::vector<T> data_;
};

// ---------------------------------------------------
// Perfect Forwarding Example
// ---------------------------------------------------
std::vector<std::string> V;
void insert(const std::string& s)  {
	std::cout << "Copy constructing object " << s << "\n";
	V.push_back(s);
}
void insert(std::string&& s) {
	std::cout << "Move constructing object " << s << "\n";
	V.push_back(std::move(s));
}
template <typename T>
void insertValue(T&& value) {
    // Forward the value preserving its lvalue/rvalue
	// thus either copying or moving the data
	insert(std::forward<T>(value));
}

// ---------------------------------------------------
// std::function
// ---------------------------------------------------
void print(const std::string& s) {
	std::printf("%s\n", s.c_str());
}
std::vector<std::function <void(const std::string&)>> F = {
	print,
	[](const std::string& s) {
		std::cout << s << " from lambda\n";
	}
};


int main() {

    std::cout << "--- Functor: MaxTracker ---\n";
    // Create a vector of integers.
    std::vector<int> values = { 10, 5, 15, -1, 20, 18 };

    // Initialize MaxTracker with the first element.
    MaxTracker tracker(values.front());

	
    for (const auto& val : values) {
        if (tracker(val)) {
            std::cout << "New max found: " << tracker.current_max << "\n";
        }
    }
	std::cout << "\n";
    std::cout << "--- Lambda: Print Elements ---\n";
	std::string outsidestring{"Printing: "};
    // Define a generic lambda to print an element.
    auto print = [&] (const auto& element) mutable -> void {
        std::cout << outsidestring << element << "\n";
    };
    std::cout << "Vector values:\n";
    for (const auto& element : values) {
        print(element);
    }
    std::cout << "\n\n";

    std::cout << "--- Move Semantics: Resource ---\n";
    // Create a Resource from a vector of ints.
    std::vector<int> resourceData = {1, 2, 3, 4, 5};
    Resource res1(resourceData);
    res1.print("res1");
    // Move res1 into res2.
    Resource res2(std::move(res1));
    res2.print("res2");
    // Note: res1 is now in a valid but unspecified state.
	res1.print("res1"); // this is not safe in general!!!
	
	std::cout << "\n";
    std::cout << "--- Perfect Forwarding ---\n";

    const std::string s1{"Hello"};
	std::string s2=", ";
    // Passing an lvalue:
    insertValue(s1);
	insertValue(s2);
    // Passing an rvalue:
    insertValue("World!");

	for(const auto& s: V)
		std::cout << s;
	std::cout << "\n";

	std::cout << "\n";
    std::cout << "--- std::function ---\n";
	F[0]("Hello");
	F[1]("Hello");
		
}
