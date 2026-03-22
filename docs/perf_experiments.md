# Performance Experiments

Internal log of the main performance experiments in `qdata-cpp` for future reference.

## Benchmark Setup

- Main benchmark: `qdata_benchmark`
- The default benchmark now tracks the production path (`write` + `io_read`).
- Typical large run:
  - `ROWS=10000000`
  - `REPS=5`
  - `shuffle=true`
  - thread grid `{1,2,3,4,8}`
  - compress grid `{3,9}`
- Smaller tuning run:
  - `ROWS=1000000`
  - `REPS=15`

## Graph Modes

- `io_graph`: overlap file IO and compression/decompression in the background while the caller still does the final recursive serialize/deserialize step.
- `full_graph`: experimental alternative that moved the final deserialize step itself into the graph.

Notes:

- On the R side, `io_graph` is a hard requirement because interaction with the R API requires object allocation and construction on the main thread.
- On the C++ side, `full_graph` was possible because we could build alternative reader machinery and benchmark it directly.
- `full_graph` is now archived under `docs/experimental/` and is no longer part of the active build.

## Experiments

### Write-side Full Graph

- No benefit. After pushing block data to the graph, main thread joins the graph. Conceptually, no downtime. 

### Read-side Full Graph

- Motivation:
  - `io_graph` read had a busy-wait handoff on `get_new_block()` where it waits for new blocks
  - main thread could spin while waiting for the next decompressed block
- Approach:
  - built a separate full-graph read path that included serialization as another node in the graph.
  - replaced recursive parser state with an explicit state machine
- Result:
  - Overall worse than `io_graph`
  - zstd decompression is very fast, so there's very minimal busy-wait
  - Extra graph and state machine complexity likely introduced more overhead than it was worth

### `-O3`

- Tried forcing `-O3` everywhere.
- Result:
  - did not improve the basic thread scaling
  - Performed significantly worse than default -O2 overall

### Profiling

At high threads, there is negative performance scaling on the read side

- Tools used:
  - Linux `perf stat`
  - Linux `perf record`
  - `perf report`
- Profiled `full_graph` read with `perf` to troubleshoot negative thread scaling on the read side
- Main findings:
  - `2` threads looked healthy
  - `8` threads showed a lot of TBB scheduling / wait overhead
  - decompression was not the main problem
  - string handling and allocation were real costs
- Outcome:
  - Improvements on negative scaling focused later tuning on backpressure / pipeline shape, not on compiler flags
  - unlikely that there is a full fix for negative scaling

### Read-side Concurrency Tuning

- Tried:
  - hard cap on decompressor concurrency
  - limiter node before decompression to limit pileup
  - inflight window sweeps
- Main findings:
  - tiny inflight windows underfed the parser
  - very large inflight windows reintroduced run-ahead overhead
  - moderate finite windows worked best
  - hard decompressor caps were less portable / less convincing
- Outcome:
  - preferred finite inflight limiting over thread-count-based caps
  - settled on a moderate inflight limit of 12

### `pool_read` / Hybrid Reader

- Tried several hybrids around dedicating a thread to read data from disk, instead of leaving it on the graph 
- Result:
  - Generally worse performance

### String Read Layout

- Explored replacing `std::optional<std::string>`-style string storage with a read-optimized blocked layout and `string_view` access.
- Result:
  - major read-side speedups, switched to that with a `string_view` iterable interface

### Direct String Write Route

- Tried a direct string fast path that emitted preserved encoded string records.
- Result:
  - the direct route added complexity but little or no real benefit
