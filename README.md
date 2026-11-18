# Dynamic Memory Trees

Experimental C++23 implementation of compact dynamic bit storage. The current
milestone provides a CPU `DynamicBitVector` built from bounded bit segments,
packed size-class cells, 32-bit relative handles, and an AVL index.

Build and test:

```sh
cmake -S . -B build -DDMT_ENABLE_SANITIZERS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

`PROJECT_MEMORY.md` records the agreed scope, architectural decisions, and
deferred work for future sessions or model changes.
