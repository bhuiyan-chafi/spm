# OpenMP v.4

Read the [v.1](./openmpv1.md), [v.2](./openmpv2.md) and [v.3](./openmpv3.md) first to keep yourself aligned.

## Legends

- num_workers * n -> to create more tasks

## Issues we solved

We solved some of the issues from v3 but we cannot say v4 is the optimal solution. That is why, we have more versions to come with solutions of each of our problems.

### Introduced Queues with mutex locks

In this version we introduced queues for tasks. One queue is used to push tasks to be processed, another one is to push tasks that are already processed.

### Created more tasks so that workers remain busy

We used `workers * n` to generate more tasks for one DISTRIBUTION_CAP, so that the workers get more tasks to remain busy. The intention was to read more data while the workers are busy(achieving more synchronization).

## New issues introduced by the solution

- what should be the optimal value for DISTRIBUTION_CAP?
- we used tasks = `workers * n` -> what should be the optimal `n`?
- No queue size
- now we are writing intermediate file = MEMORY_CAP; slow write, because of what the tasks queue is getting bigger and bigger.
- memory is not released immediately
- introduced IO contention
