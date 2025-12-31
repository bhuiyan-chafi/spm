# OpenMP v.4

Read the [v.1](./openmpv1.md), [v.2](./openmpv2.md) [v.3](./openmpv3.md) and [v.4](./openmpv4.md) first to keep yourself aligned.

Well I would say from this version we are having parallelism, from now on we will do optimization only because the main skeleton is ready.

## Issues we solved

- we solved the unbounded read by the `emitter` by introducing `queue size` for sorted and unsorted tasks. Now we can put back-pressure to the emitter till the queues gets empty. This is mostly beneficial when we are performing OOC(when input size exceeds MEMORY_CAP) operation.

## New issues introduced by the solution

- we didn't introduce new ones, the old ones are still there.
- but one thing we can indicate is that: what should be the optimal queue size?

## Conclusion

In this version the `Collector` is bottleneck.
