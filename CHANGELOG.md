## 1.1.0 - 2026-04-08

### Format compatibility and limits

- enforced the R-compatible size limits on both read and write:
  - vector and list lengths capped to the R-compatible `R_XLEN_T_MAX` range
  - attribute counts capped to the R-compatible `R_LEN_T_MAX` / `INT_MAX` range
  - string payload and attribute-name lengths capped to `INT_MAX`
- added native recursion-depth protection with a configurable `max_depth`
  parameter and a default of `512`
- documented that native `qdata-cpp` preserves attributes structurally as
  `name + object` pairs and does not try to emulate R's special attribute-setter
  semantics

### Serialization internals

- replaced the old templated in-memory writer core with a shared erased writer
  path
- kept the public templated buffer-facing serialize surface on top of that
  erased writer implementation
- tightened the installed include tree so the standalone `include/` root is
  self-contained for downstream consumers

### Testing and vendoring

- added native regression coverage for the compatibility limits and `max_depth`
  behavior
- updated the vendored `xxHash` copy from `0.8.2` to `0.8.3`

## 1.0.0 - 2026-04-07 - commit `1d21f34dbcaa`

- initial release
