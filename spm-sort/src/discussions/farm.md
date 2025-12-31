# Solving with FastFlow

The program can be [found here](../farm.cpp).

The same problem following the same pattern I solved it with FastFlow. The frame is designed by my Professor Massimo Torquati, and to be honest it's gives you more flexibility then OpenMP. Both of the APIs are fantastic but in my opinion FastFlow gave more flexibility and less synchronization problem. I will discuss my findings below.

## Maintaining Queues

I had to use queues to maintain synchronization(back pressuring stages) between emitter, worker and collector. Specially for emitter and workers. Maintaining those queues introduced a couple of variables and a lot of synchronizations in each stages.

## Better Distribution

Since FastFlow can handles distribution based on `set_scheduling_ondemand()`, I have just put the degree level of my distribution there. `set_scheduling_ondemand()` set's the workers in a notify mode, whenever a worker is idle it get's a task from the emitter. No lock, no manual queues nothing, just pure FastFlow and it does the magic for you. While programming, working with manual resources are risky because a bad implementation or un-optimized code can drastically change the performance. FastFlow in that sense is the best parallel tool we can get.

## Performance

If we compare the performance, which I did! It is not significantly lower than OPEN_MP. I will not say my implementation is good enough to judge any of the libraries but it gives a primary idea. The program that I designed can still be optimized a lot, and maybe if it is perfectly optimized; we can achieve a better performance using FastFlow. Right now it has a difference of milliseconds.

## Future Work

The program still needs optimizations on:

- data structure
- communication overheads
- memory consumption
- parallel writing in single file(I didn't do it, in the main report I have explained why).
- more tuned value of the `DEGREE` and `DISTRIBUTION_CAP`

I hope if someone becomes interested in future and does something on these parts, please send me an email with your findings.
