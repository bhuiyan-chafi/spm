# Lambdas in C++

In C++, **lambdas** are a way to create small, unnamed functions directly where you need them — without separately writing a full `function` or `struct`/`functor`.
They were introduced in **C++11** and have become very common in modern C++.

---

## **Basic Syntax**

```cpp
[ capture_list ] ( parameter_list ) -> return_type {
    // function body
}
```

* **`capture_list`** → tells the lambda what variables from the surrounding scope it can use.
* **`parameter_list`** → like a normal function's parameters.
* **`return_type`** → optional; usually deduced automatically.
* **function body** → what the lambda does.

---

## **Example 1 – Simple Lambda**

```cpp
#include <iostream>
using namespace std;

int main() {
    auto greet = []() {
        cout << "Hello from lambda!" << endl;
    };

    greet(); // call it like a normal function
}
```

---

## **Capturing Variables**

There are different ways to let the lambda use variables from the outside:

```cpp
#include <iostream>
using namespace std;

int main() {
    int x = 10, y = 20;

    auto sum_by_value = [x, y]() {   // copy values
        return x + y;
    };

    auto sum_by_reference = [&x, &y]() {  // use references
        return x + y;
    };

    cout << sum_by_value() << endl;      // 30
    cout << sum_by_reference() << endl;  // 30
}
```

**Common capture modes:**

* `[=]` → capture **everything** by value (copy).
* `[&]` → capture **everything** by reference.
* `[x]` → capture only `x` by value.
* `[&x]` → capture only `x` by reference.
* `[=, &y]` → capture everything else by value, but `y` by reference.

---

## **Example 2 – With Parameters**

```cpp
#include <iostream>
using namespace std;

int main() {
    auto add = [](int a, int b) {
        return a + b;
    };

    cout << add(5, 3) << endl; // 8
}
```

---

## **Example 3 – With Return Type**

If the return type can’t be deduced automatically (e.g., different branches return different types), you can specify it:

```cpp
#include <iostream>
using namespace std;

int main() {
    auto half = [](double x) -> double {
        return x / 2.0;
    };

    cout << half(5.0) << endl; // 2.5
}
```

---

## **Why Use Lambdas?**

1. **Short, inline logic** without defining a separate function.
2. **Custom sorting, filtering, callbacks**.
3. **Closures** — they can remember variables from where they were created.
4. Often used in STL algorithms like `std::for_each`, `std::sort`, `std::find_if`.

---

## **Example 4 – With `std::sort`**

```cpp
#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    vector<int> v = {5, 1, 4, 3, 2};

    sort(v.begin(), v.end(), [](int a, int b) {
        return a > b; // sort in descending order
    });

    for (int n : v) cout << n << " ";
}
```

**Output:**

```cpp
5 4 3 2 1
```

---

If you want, I can also show you **how lambdas relate to functors**, because under the hood a lambda is basically a **compiler-generated functor**.
Do you want me to go into that?
