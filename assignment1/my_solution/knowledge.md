# Talking about the `intrinsic` here

We will discuss `intrinsic` of vectorization here. For example: _m256, load and store, copy, move etc.

## The `__m256` type

A `__m256` is basically a **compiler-provided type that maps directly to a 256-bit YMM register**.

* It’s **not** an array.
* It’s **not** a pointer.
* You can’t “index” it.
* You can’t “take the address” of one of its lanes.

It’s literally just a 256-bit blob that the intrinsics know how to interpret (as 8×float, or 4×double, or 32×int8, etc., depending on the intrinsic).

If you want random access to the lanes → you spill it to memory (`_mm256_storeu_ps` → array).
If you want to move it around → you pass it by value (like a struct).
If you want to reuse lanes in registers → you use **shuffle/extract intrinsics**.
