# OpenMP v.3

Read the [v.1](./openmpv1.md) and [v.2](./openmpv2.md) first to keep yourself aligned.

## Legends

- DISTRIBUTION_CAP = size/memory_cap = unit/workers

## Issues we solved

We solved some of the issues from v1 and v2 but we cannot say v3 is the optimal solution. That is why, we have more versions to come with solutions of each of our problems.

### Achieved some degree of parallelism(pipeline) with DISTRIBUTION

In this version we have used the DISTRIBUTION_CAP to force the emitter reading this amount of data at a time. Since the emitter cannot read more than the DISTRIBUTION_CAP, now we don't have the `memory sky rocket issue`.

So, now the workflow is [emitter]->[read = DISTRIBUTION_CAP]->[workers]->[write/send to collector]->[collector]->[output].

But this is not optimal, why?

- because while one DISTRIBUTION_CAP is getting processed, the emitter is paused.
- then the next DISTRIBUTION_CAP is read and at that time the workers are paused.
- so this some sort of synchronization but is not something we wanted to achieve.

## Issues still remaining

- vector overheads[this needs additional support, but I don't think I will have the time to solve it perfectly]
- idle workers because distribution is not perfect

## New Issues introduced by this versions

To be honest it didn't introduce additional issues rather than solving the DISTRIBUTION one.
