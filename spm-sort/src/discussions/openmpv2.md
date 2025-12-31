# OpenMP v.2

Read the [v.1](./openmpv1.md) first to keep yourself aligned.

## Issues remained from the previous version v.1

- [vector overhead](./openmpv1.md/#fails-if-you-have-a-input--memory_cap--nearly-equal-to-your-machines-physical-memory)
- [idle workers](./openmpv1.md/#idle-workers)
- [no distribution](./openmpv1.md/#no-distribution-of-the-data)

### The MEMORY sky rocket issue due to NO DISTRIBUTION

I was testing v2 with a 100M 16 bytes long payload data input. The file size was 2.7GiB but right after the read I was noticing a memory consumption of 5.9GiB which was something shocking. Why is that?

Well, the findings were obvious:

- struct Item where `struct` has it's own memory overhead, the cost is not insignificant(16 bytes).
- there were some metadata due to memory allocation which was 8~12 bytes(assumed).
- the growth of vector that we already discussed.

In total these resulted a total overhead of 64 bytes whereas were counting around 36 bytes for each item. You can check this [program](./tests/memory_overhead_test.cpp) which actually proves this point.

**This problem has been partially solved in the next version but to be honest we can do more work on this if we want, but I didn't have the time even though I had some interests in it. So, if anyone in future have time and solved it(except distribution cap in v3, something with optimizing data structures) please send me an email with your solution.**

## Issues that we solved

- [program failure](./openmpv1.md/#fails-if-you-have-a-input--memory_cap--nearly-equal-to-your-machines-physical-memory) due to large size input size

Now after each intermediate write, we are freeing the memory so new segment reads are allocated without paralyzing the machine. So, we achieved some sort of parallelism.

## New issues introduced

It is true that to find a perfect trade-off, if we solve some problems the solutions itself bring new ones. Here happened the same:

- [more intermediate writes] : now that we are writing intermediate segments the time for workers increased because -> sort+write by each worker. Emitter is now waiting a long time for OOC operation, also other workers who finished early are waiting because there is nothing to do.

## Trade-off point

Now that we are writing intermediate segments, the question is who should write them? We had two options:

- multiple workers write into one segment = MEMORY_CAP and finally collector merge those segments to one output file.
- one worker write it's own slice after sorting, which results increased number of intermediate writes. Is that bad? Well, multiple writers can write in a single file but we need mutex lock for that and then we will introduce thread acquire contention(race condition) which will create more communication overhead.
- so we decided to write intermediate slices by the workers and one collector.
