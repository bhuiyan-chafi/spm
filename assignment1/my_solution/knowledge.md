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

## Mistake that I made with the Input Size

Our operation was with FP32, 32-bits floating point which is 4bytes. So, the question is what is the maximum value we can accumulate within a float?

Is a float can contain a numerical value 2^32=4294967296? The answer is no! Because all the 32bits in a FP32 is not for numerics. If we divide these 32 bits we get 1(sign-bit), 23(mantissa-bits) that contains the numerical value and 8 exponents values. So, 2^23=8388608 is the value but we have 1 implicit bit that gives us a 24bit mantissa that can contain a value 2^24=16,777,216.

But if we surpass 2^24 then the gap becomes 2 instead of one for the next number. For example we can represent 16,777,216 but we cannot represent 217. We have to represent 218. This is called ULP(Unit in the last place). 

Explaining the ULP: we have 23 mantissa built in, with one 1 implicit bit it's 24. So till 24 bits we have the 23 bits mantissa and the least significant bit is 2^0=1. When we surpass 24 bits we borrow another bit from the exponent and now the least significant bit is 2^1=2 and the gap becomes 2. For example if we have 7 digits display then the number we can show is 9,999,999 but if we add just one more number it becomes 10,000,000 which are 8 digits which our display doesn't have. Now if you surpass 25 bits then the resolution will become 2^2=4 and then 2^3=8 and so son. Each bit you borrow it will increase the difference.
