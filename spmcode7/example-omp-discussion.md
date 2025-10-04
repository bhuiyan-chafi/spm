# Learning about new features of c++ and openMP

In each code there is something new for me which I need to know what is it about. So, here I will discuss some features from my professor's code. Let's start with:

## using task_t=std::pair<float, size_t>

Where `task_t` nothing but a pair of items. When we say pair, we strictly mean `2` items not more than that. For example we can do this as pair:

```cpp
task_t myTask = {3.14f, 1000};
std::cout << "First = " << myTask.first<< ", Second = " << myTask.second << "\n";
```

Attributes:

- strictly two items
- mutable

In this code, prof. has just declared it but never used it. 

## `inline float dotprod()`

Inline functions are declared to make the function live at call site(from where each time the function is called) so that each time the compiler doesn't have to call it from it's place. But in our case it's not much of a use because if we use -O3 the compiler already does this for possible functions. The use-case is effective in header files which are used in several programs. The compiler also `merges` several identical inline functions together to avoid different definition problem. 

## 	ssize_t nworkers = omp_get_max_threads()

Here `nworkers` are just number of workers we are declaring for our program. If we don't assign a value later, initially it returns the number of logical cores available in our machine. For my UBUNTU machine it's 4(2 core, 4 thread). 

In code the prof. has assigned fixed number of workers as per input:

```cpp
if (argc==3) {
    nworkers = std::stol(argv[2]);
    if (nworkers<=0) {
        std::cerr << "nworkers should be greater than 0\n";
        return -1;
}
```

## The `auto random01` lambda

Is used to generate the random numbers, there are a few things to know based on the implementation.

```cpp
auto random01= []() {
    static std::mt19937 generator;
    std::uniform_real_distribution<float> distribution(0,1);
    return distribution(generator);
}; 
```

### `static std::mt19937 generator`

`static` otherwise each time it's called from the loop, the generator engine restarts and we get duplicate values. In our code we called the function from inside a loop. 

### std::uniform_real_distribution<float> distribution(0,1)

Is a class template where `distribution` is the object. We didn't use simple float generator because, generic random number generators give us uniform numbers. Well, for testing it is not a bad idea but in real programs we use numbers that are really different from each other.Declaring an uniform_distribution gives us a wide range of floating points. So, the engine is what keeps the random integers aligned that are later fed to the uniform_distributor(divides the integer by max size 32bits for this case) which generates the floating random number. 

## float sum{0.0}

Is the latest c++ initialization practice(good practice). It functionally means the same thing float sum = 0.0 but there is a subtle difference. For example if we do something stupid like:

```cpp
int sum {3.75}
```

It will generate type narrowing error because it's not allowed, but if we do this:

```cpp
int sum = 3.75
```

It will allow and slightly narrow the value, which is not good.

## #pragma omp single

Tells openMP that only one thread should be used here. This part is inside the `#pragma omp parallel num_threads(nworkers)` part, and the undersigned part is just assignment of values to the vectors. If we don't specify this one thread execution, all the threads will be in a race to assign values which will eventually produce wrong assignments. 

The rest of the part is pretty generic, which are already discussed through the lecture notes. And finally we do vector multiplication and summation of the results. One thing, though! Don't get confused with the time output, yes for same input it generates different time outputs; it is because the vector sizes are variable(random in a sense) so the CPU handles different load of computation each time.