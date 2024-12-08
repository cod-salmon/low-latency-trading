# Memory Pool
The `MemPool` class is defined within a template of typename T, which sets the memory type for the pool. The `MemPool` class consists of:
- an `ObjectBlock` struct, containing an empty T instance, and a bool variable `is_free_`, that changes to false when T is initialised during `allocate`.
- A vector of `ObjectBlock` objects called `store_`. The vector size gets set during the construction of the `MemPool` instance (the `MemPool` constructor takes a std::size_t to specify the size of the vector of `ObjectBlock` objects). The `store_` vector effectively constitutes the memory pool: after the `MemPool` instance gets built, we have std::size_t memories of type `ObjectBlock` to be filled in.
- a std::size_t `next_free_index_`, to keep track of the next `ObjectBlock` memory in `store_` which is empty and ready to be filled in.
- an `updateNexrFreeIndex` function to update `next_free_index_` after a new `ObjectBlock` instance has been placed in the `store_` through `allocate`.
- an `allocate` function which allocates some memory for an object of type T with the specified set of parameters `args`. It does so by accessing the `next_free_index_`th  `ObjectBlock` element in the `store_` vector and place the new T(`args`) onto that memory; then it calls `updateNexrFreeIndex` to update the `next_free_index_`.
- a `deallocate` function, which resets the `is_free_` boolean of the `store_`'s `ObjectBlock` element at the given T's index within `store_`: first it finds where in `store_` lies the input T (some index); then it uses that index to access its respective `ObjectBlock` memory in `store_` and reset its boolean `is_free_` variable to true.

The first output lines for the the `mem_pool_example.cpp` are:

```
low-latency-app  | prim elem:0 allocated at:0x570e0a6eeeb0
low-latency-app  | struct elem:0,1,2 allocated at:0x570e0a6ef220
low-latency-app  | deallocating prim elem:0 from:0x570e0a6eeeb0
low-latency-app  | deallocating struct elem:0,1,2 from:0x570e0a6ef220
```