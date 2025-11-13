# OpenMP

Read the [v.1](./openmpv1.md), [v.2](./openmpv2.md), [v.3](./openmpv3.md), [v.4](./openmpv4.md) and [v.5](./openmpv5.md) first to keep yourself aligned.

This is the final version that I build, which is not 100% perfect but according to my knowledge covers most of the topics we covered throughout the course.

## Legends

L1_CACHE: L1 cache size of the executing machine.

DEGREE: constant that decides how many internal slice should be made to achieve a size = L1 cache
    = WORKERS^WORKERS

DISTRIBUTION_CAP = L1_CACHE * DEGREE

WRITERS = WORKERS/2, because most of the time collector is paused, when the queue is full emitter is paused, while there is no task and emitter is reading, some workers are sleeping. So, I decided to use half of workers and let OS do the scheduling. Increasing this number introduces more IO contention. But, yes there are chances to improve this.

QUEUE_SIZE = DEGREE*WORKERS, because emitter is WORKERS times slow. Making it read WORKERS time more will give some advantage in future segments, except the first one. But, can be improved by studying more.

## Future work

If anyone wishes they can work on the `memory OVERHEAD`, `optimal DISTRIBUTION_CAP`, `optimal DEGREE`, `optimal intermediate WRITERS`.

## Issues we solved

- emitter is now more faster
- workers can adjust an entire slice in their L1 cache so sorting operation now is super fast
- intermediate writers are writing from the sorted task queue which let's emitter and worker keep working

## New issues introduced by the solution

- more slices, more communication overhead
- MEMORY_OOC have too many intermediate writes for large files(each of DISTRIBUTION_CAP size)
- too many intermediate write makes the collector a bit more slow

## Conclusion

Final finding for me is the `overhead` and `collector` being slow. No process is bottleneck during the process.
