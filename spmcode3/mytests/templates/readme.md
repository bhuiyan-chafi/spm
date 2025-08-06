# Why the `templates`?

`template` is a feature.

In such scenario where we cannot determine the type of data we will be handling, we use the template.

```cpp
    template <typename T>
    T add(T a, T b){
        return a+b;
    }
```

We can have multiple parameters if we want:

```cpp
    template <typename T, typename U>
    auto addWithDifferentType(T a, U b)
    {
        return a + b;
    }
```

If we pass different generic data-types, the template function will handle them at compile time. For example:

```cpp
    auto result = add(5, 10);
    result = add(5.5, 10.5);
    result = addWithDifferentType(5.5, 10);
```

Will be handled as `int` and `float` at compile time.

## why didn't you use `auto` in the first one?

Well! auto is not for dynamic typing but type deducing. In other words writing clean code when you have a complex data-type.

>vector<vector<>,vector<>> something;
