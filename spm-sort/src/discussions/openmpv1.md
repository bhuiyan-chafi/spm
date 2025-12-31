# OpenMP v.1

File: [openmpv1.cpp](../openmpv1.cpp)

`The algorithm is in my Tablet, which I have to draw properly and then add the image here. So, skip it for now.`

Legends:
    - **MEMORY_CAP** : as per project description it should be 32GiB but it's variable so you can change it while executing. It
    lets you test if your program stops if memory is exceeded.
    - **DISTRIBUTION_CAP** : bytes of data sent to the workers to process
    - **OOC** : Out of Core defines exceeding MEMORY_CAP

## Issues

The program is not what we expected it to be, when I was studying and practicing I designed this with help of AI(of course). I learned that AI is only helpful when you have a clear concept of the topic, otherwise you will end up designing stupid programs. Yes, it takes a lot of time to know all the functions, techniques of a modern programming language! In that sense AI is helpful but it is you who should know what you are doing. So:

### No DISTRIBUTION of the data

- If we clearly look at the workflow, the emitter is reading everything from the memory(size == MEMORY_CAP) and then the workers start processing them. In that sense it's not parallelism at all. The only parallelism we achieved here is, dividing those items(records) into slices and tasks and then processing them in parallel. Which is bad(because the motto is: whatever can be parallelized should be parallelized). Check from line:94, to see the implementation of the emitter.

- Another major issue due to no distribution of the data is that, when we start reading the data and read till MEMORY_CAP the memory consumption is **sky rocketed**. Versions until v3 has this same issue, which is fixed in v3. [In v2](openmpv2.md/#the-memory-sky-rocket-issue-due-to-no-distribution) I have discussed this in detail.

### Idle Workers

This issue is obvious when there is no DISTRIBUTION_CAP. Workers remain idle until the emitter is done reading everything from the memory.

### No Pipeline

If we look at the OOC(line: 134) we can see that the segment=MEMORY_CAP is sent to the collector to perform the K-way merge. After the merge the collector writes that segment and the functions scope ends. The emitter and all the workers are idle at that time. In this case if you are processing 20GiB of data with a MEMORY_CAP=2GiB then it's a disaster.

A more severe issue has been [discussed here](#fails-if-you-have-a-input--memory_cap--nearly-equal-to-your-machines-physical-memory).

### Static Scheduling of Tasks

In this program the scheduling is to read a segment -> ranges = segment/workers -> slices = range[i] where we have multiple Tasks. One slice is given to a worker and that distribution is done with `#pragma`

```cpp
// Line 196
schedule(dynamic) num_threads(static_cast<int>(threads))
```

which is useless here because ranges=workers. Since the payload is of dynamic size and keys are random but still the difference is not that much that we can get benefitted by a dynamic distribution.

### Fails if you have a input > MEMORY_CAP = nearly equal to your machines physical MEMORY

- See this [discussion first](./Payload.md) then proceed below.

- If we see the implementation it is very clear that we are allocating vectors to keep our items there.
- But we are not freeing them until the whole program is executed.
- Vectors in c++ are dynamic and has an allocation factor of `2^n where n=0,,1,2,4....N`. So, if we want to put 10 items in a vector the size of that vector is not 10, its 2^4 = 16. Which is 6 extra allocation of the content size. Our `Item` struct has a minimum size of `uint64_t + uint32_t + payload` which accumulates nearly 200~268 bytes. So, 6 items of 268 bytes is 1608 bytes = 1.6KiB. Imagine having 10/100M of items!!
- Test this [program](../../tests/vector_capacity_test.cpp) to verify the claim.
- And this causes to hanging up your entire machine if your input size is nearly equal your memory size(in my case it was 100M records with a payload size 128 bytes ~ 7.8GiB).
- **[critical]** It is because I am reading a segment till MEMORY_CAP(4GiB) and then send this as tasks to the worker, where more overheads were created and the emitter at that time has already started reading the next segment. Which resulted 7.8GiB read + overheads but by that time we are not processing anything because we didn't achieve pipeline parallelism in this version.

It was a relief that I decided to test first in my machine not the spmcluster! Otherwise I would have been paralyzed the cluster. Always run in your machine first, at least the single node versions.
