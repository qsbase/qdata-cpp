# Full Graph Archive

`full_graph` was an experimental alternative to the production `io_graph` reader.

Core idea:

- keep ordered decompressed blocks inside the TBB graph
- move final deserialize into the graph instead of the caller
- replace the recursive reader with an explicit parser state machine

What we learned:

- it worked correctly
- it never became a clear overall winner
- profiling showed the remaining bottleneck was mostly scheduler/runtime overhead and the serial parser itself
- once the string/read-side work improved, `io_graph` remained the better production path

Why it is no longer in the active tree:

- we settled on a single shared `io_graph` path for both `qs2` and `qdata-cpp`
- keeping the extra graph mode in the active build added maintenance cost and extra plumbing

Rough shape of the experiment:

```cpp
reader_node -> decompressor_node -> sequencer_node -> deserializer_node
```

Notes:

- the old full-graph implementation lived in the vendored `qdata-cpp` tree before this cleanup
- if we revisit the idea later, git history and the performance notes in [perf_experiments.md](../perf_experiments.md) are the reference
