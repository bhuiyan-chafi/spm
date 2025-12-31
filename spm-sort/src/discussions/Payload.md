# The most stupid thing that I did

In the project description it was mentioned to declare pointers for storing the payload since its dynamic. But due to flexibility I declared `std::vector<>`, which skyrocketed my memory consumption and resulted in bricked program for large dataset.

The structure of one `Item` was:

```cpp
struct Item
{
    uint64_t key;
    // the main culprit
    std::vector<uint8_t> payload;
};
```

And then I declared another `vector` to put `Item`s to put them after reading:

```cpp
while (true)
{
    uint64_t key;
    std::vector<Item> items;
    if (!read_record(in, key, payload))
    {
        break; // EOF
    }
    items.push_back(Item{key, std::move(payload)});
}
```

What is happening here? If we look closely we can see we are declaring `vectors` of `vectors`:

`std::vector<Item> items -> Item{key, std::vector<uint8_t> payload}`, the vector has 24bytes of overhead which for 10M records creates around 228MiB of overhead before storing any item in the bucket vector. To verify the claim execute [this program](./tests/vector_overhead_test.cpp).

So, instead of vector we used `pointers`(a class declared as CompactPayload which accepts variable length) to reduce this overhead. The pointer consumes only 8 bytes which saved 16bytes. For large number of records it is a significant save. You can test [this program](./tests/vector_vs_pointer.cpp) to verify the claim.
