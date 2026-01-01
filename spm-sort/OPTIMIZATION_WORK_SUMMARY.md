# FastFlow Farm Optimization Work Summary

## Project Context

You're working on a **FastFlow-based external sorting implementation** (`/home/chafi/spm/spm-sort/src/farm.cpp`) that handles large datasets that exceed memory capacity. The system reads data from disk in segments, distributes them to workers for parallel sorting, and merges the results.

## Architecture Overview

- **Emitter**: Reads records sequentially from disk, batches them into segments of size `DISTRIBUTION_CAP` (8 MiB), slices each segment into `DEGREE = WORKERS²` parts, and distributes tasks to workers
- **Workers**: Sort individual slices using `std::sort()` and measure their individual CPU time
- **Collector**: Aggregates sorted slices by segment_id and performs k-way merge when all DEGREE slices are received

## Phase 1: Initial Problem Assessment (Messages 1-2)

**Goal**: Understand the current farm implementation and identify performance metrics needed.

- Reviewed the farm.cpp architecture
- Identified that optimal worker count follows formula: $k_{opt} = \frac{T_{arrival}}{T_{service}}$
- Needed to measure:
  - **T_arrival**: Time between consecutive segment emissions
  - **T_service**: Time for workers to process one segment

## Phase 2: Inter-Arrival Time Instrumentation (Messages 3-7)

**Goal**: Add spdlog logging to measure inter-arrival time and validate the optimal worker formula.

### Iteration 1 (Message 3-4)

- Added `TimerClass inter_arrival_timer` to Emitter struct
- Initialized timer in `svc_init()`
- Started/stopped timer around segment completion
- **Issue Discovered**: Timer was measuring through queue contention, not pure I/O

### Iteration 2 (Message 5)

- **Root Cause**: Timer started inside `emit_segment()` lambda after all slicing was complete
- **Fix**: Moved timer start to beginning of segment reading
- Measured: Start of reading segment N → Start of reading segment N+1
- **Result**: Eliminated queue contention from measurement

### Iteration 3 (Message 6-7)

- Confirmed timing was now measuring pure inter-arrival (segment read start to next segment read start)
- Times stabilized: ~55-62ms independent of worker count
- This represented true I/O arrival rate

## Phase 3: Accumulated CPU Time Tracking (Messages 4-8)

**Goal**: Measure pure work time for workers to validate service time.

### Implementation

- Each worker accumulates its sort time: `timer_work.add_elapsed(slice_timer.elapsed_ns())`
- Returns `sort_time_ns` in `TaskResult`
- Collector sums all slice times for a segment: `seg.total_cpu_time_ns += result->sort_time_ns`
- Logs when segment complete: `"Segment {} accumulated CPU time: {}"`

### Key Insight

- Accumulated CPU time / number of workers = average service time per worker
- With 1 worker: ~5-6ms per segment
- With 4 workers: 5-6ms accumulated, so ~1.3ms per worker
- With 48 workers: varying but much smaller per-worker time

## Phase 4: Parallel Wall-Clock Time Investigation (Messages 8-12)

**Goal**: Understand timing anomalies when scaling workers.

### Problem Observed

With 48 workers, parallel wall-clock time was **212.843 ms** but accumulated CPU was only **29.480 ms** (7x gap!)

### Root Cause Analysis

- Initial approach: Measured from `earliest_start_ns` (first worker starts) to `latest_end_ns` (last worker finishes)
- This included:
  - Task scheduling delays
  - Queue contention from creating 2304 slices per segment (DEGREE = 48²)
  - OS context switching overhead
  - All communication and synchronization overhead

### Key Discovery

- Real service time = accumulated CPU / num_workers = 29.480ms / 48 ≈ 0.6ms per worker
- Parallel wall-clock time measures something different: overhead, not pure work
- This metric was **misleading** for calculating optimal worker count

### Decision

Removed parallel wall-clock time tracking entirely. Focus on **pure CPU time** only.

## Phase 5: Simplification and Cleanup (Messages 13-14)

**Goal**: Remove complexity and measure only what matters.

### Removed

- `inter_arrival_timer` from Emitter (was adding complexity without new insights)
- `slice_start_ns` and `slice_end_ns` from TaskResult
- `parallel_timer` from Collector
- `earliest_start_ns` and `latest_end_ns` tracking
- Absolute timestamp recording in Worker

### Kept

- Disk read time (pure I/O arrival): Measured from start of reading one segment to end of reading
- Accumulated CPU time (pure work): Sum of all worker sort times per segment

## Phase 6: Disk Read Time Refinement (Message 15 - Current)

**Goal**: Measure only pure disk I/O, excluding all slicing and emitting overhead.

### Discovery

Disk read time was increasing with more workers:

- 1 worker: ~28-29ms read time
- 4 workers: ~60ms read time

### Root Cause

Timer was stopping **inside** `emit_segment()` lambda, which includes:

- Creating DEGREE slices from the segment
- Copying data for each slice
- Calling `ff_send_out()` for each slice (queue operations)

With DEGREE = WORKERS², this overhead scales quadratically.

### Current Fix

Moved `segment_timer.stop()` to **before** the slicing/emitting loop, so timer measures only from:

- Start of reading first record of segment
- To when the last record of the segment is read (DISTRIBUTION_CAP reached)

Excludes all downstream processing overhead.

## Final Metrics Framework

**Two clean, simple measurements:**

1. **Disk Read Time**: Pure I/O arrival rate
   - Logged by: Emitter when segment reading complete
   - Format: `"[Emitter] Segment X disk read time: Y"`
   - Independence: Should be constant regardless of worker count

2. **Accumulated CPU Time**: Pure work measurement
   - Logged by: Collector when all DEGREE slices received
   - Format: `"[Workers] Segment X accumulated CPU time: Z"`
   - Contains: Sum of `std::sort()` time for all slices in segment

## Formula Application

$$k_{opt} = \frac{T_{arrival}}{T_{service}} = \frac{\text{disk read time}}{\text{accumulated CPU time} / \text{num workers}}$$

**Interpretation**:

- If `disk_read_time > accumulated_cpu_time / workers`: Workers are waiting, can add more workers
- If `disk_read_time < accumulated_cpu_time / workers`: Emitter can't keep up, reduce workers or optimize I/O
- If equal: Workers stay busy and emitter doesn't bottleneck → optimal

## Code State

All changes localized to `/home/chafi/spm/spm-sort/src/farm.cpp`:

- Emitter measures disk I/O only
- Workers measure sort time only  
- Collector aggregates CPU time per segment
- Clean logging with focused metrics
- No misleading overhead measurements

## Next Steps (Your Test Plan)

Run with varying worker counts (1, 4, 7, 48, etc.) and verify:

- Disk read time stays constant (~28-30ms per 8MiB segment)
- Accumulated CPU time and inter-segment timing follow the optimal worker formula
- Actual performance (wall-clock time) matches theoretical prediction
