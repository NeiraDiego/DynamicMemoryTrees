# Dynamic Memory Trees: Project Memory

## Objective

Build compact dynamic C++ data structures that can insert and remove bits. The
long-term target is the dynamic range min-max tree for balanced-parentheses
ordinal trees described in Section 6 of Navarro and Sadakane's *Fully
Functional Static and Dynamic Succinct Trees*.

## Decisions made on 2026-11-18

- Use C++23 and CMake.
- The first executable milestone is a CPU `dynamic_bitvector`, not the full
  ordinal tree.
- Prioritize correctness, invariant checks, tests, and memory measurements over
  immediately meeting the paper's exact `2n + O(n/log n)` bound.
- Store persistent references as compact relative handles, never as raw memory
  pointers. This keeps a future GPU/serialization representation feasible.

## Implemented first milestone

- Segments hold from `L` to `2L` bits, except that a whole vector of at most
  `2L` bits is allowed in one shorter segment.
- A size-class pool packs equal-length segments contiguously in fixed `2L`-bit
  cells. A 32-bit handle encodes the size class and ordinal within that pool.
- Removing a pool entry moves the class's final entry into its place. Internal
  back-pointers update the moved AVL leaf; these pointers are CPU-only and are
  not part of the persistent representation.
- An AVL tree indexes segments by accumulated bit length. It supports access,
  overwrite, insertion, and deletion.
- If deletion creates an underfull segment, the current implementation rebuilds
  the segment partition. This keeps the invariant but is intentionally a
  correctness-first fallback, not the final `O(log n)` neighbor rebalance.

## Public scope for milestone one

- `size`, `empty`, `at`, `set`, `insert`, `erase`, `to_vector`, and memory
  statistics.
- Differential randomized tests against a plain bit vector.

## Implemented range min-max layer

- The AVL now stores per-node excess `e`, minimum `m`, maximum `M`, and the
  occurrence count `n` for `m`, using the composition formulas from the paper.
- The public API exposes aggregate values, prefix excess, `rank1`, `select1`,
  forward search for a global excess, and matching-close lookup.
- In this implementation `1` contributes `+1` and `0` contributes `-1`.

## Deferred work

- Local, worst-case `O(log n)` merge/borrow handling for underfull segments.
- Dynamic changes to `L` and the deamortized three-part technique.
- Universal lookup tables and word-parallel local rank/select/search.
- Backward search, RMQ position queries, and the remaining balanced-parentheses
  and ordinal-tree operations.
- GPU upload/view and GPU kernels.

## Design constraints for later phases

- Do not add persisted raw pointers to segment, node, or allocator layouts.
- Keep CPU-only maintenance metadata separate from serializable arena data.
- Extend AVL aggregates in-place for the range min-max tree rather than adding
  a second indexing tree.
- Benchmarks must report useful bits, cell capacity, pool metadata, and node
  metadata separately.
