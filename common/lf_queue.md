# Lock-free queues
In computer science, a **lock** or **mutex** (from mutual exclusion) is a synchronization primitive that prevents a state from being modified or accessed by multiple threads of execution at once. Therefore, when we talk about **lock-free** queues, we expect queues or vectors which elements can be written and read by multiple threads at once. We use `std::atomic` to define variables that allow such behaviour:
- `next_write_index_`, which keeps track of the next index where new data should be written to;
- `next_read_index_`, which keeps track of the next index where new data should be read from; and
- `num_elements_`, which keeps track of the number of elements to be read from the queue.

The `LFQueue` class is defined within a template of typename T. When the constructor gets called, a `store_` vector of type T gets assigned a size `num_elems`. Each of the T objects in `store_` gets accessed through:
- `getNextToWriteTo`, which returns a pointer to the next memory space in `store_` available to fill in. Once this is returned, one might need to call `updateWriteIndex`, so that we update `next_write_index_` and increment the `num_elements_` to be read. For example, in `lf_queue_example.cpp` we find:
```
const MyStruct d{i, i * 10, i * 100}; // Create T (a MyStruct) instance
*(lfq.getNextToWriteTo()) = d; // Assign it to the given memory space in store_
lfq.updateWriteIndex(); // Update index to write
```
- `getNextToRead`, which returns a pointer to the next memory space in `store_` holding data yet to be read. Once this is returned, one might need to call `updateReadIndex`, so that we update `next_read_index_`, and reduce the `num_elements_` to be read. For instance, in `lf_queue_example.cpp`, during `consumeFunction`, we find:
```
const auto d = lfq->getNextToRead(); // Access the next element in store_ to read
lfq->updateReadIndex(); // Update the write index
std::cout << "consumeFunction read elem:" << d->d_[0] << "," << d->d_[1] << "," << d->d_[2] << " lfq-size:" << lfq->size() << std::endl; // Output the element we've got from the store_
```

Note:
* The function `size` in the `LFQueue` class does not return the `store_` size, but the `num_elements_` to be read from the queue.
* In `lf_queue_example.cpp`, the process that reads data from `store_` gets assigned to a thread instance `ct` before the for-loop for writing data into `store_` gets called. This should mean that, once kicked in, while the process for reading data is running, the system is also writing data into the same `store_` vector. However, in this case, by using `sleep_for(5s);` inside `consumeFunction`, we allow the code to start writing into `store_` before the thread starts reading data. Thus, when calling `ct->join()`, the system should have finished writing everything that it wanted to write, and also reading everything it wanted to read. In the first few output lines from `lf_queue_example.cpp` we can see how four new elements get allocated to `store_` before the system starts reading the first:

```
low-latency-app  | Set core affinity for  139208620508736 to -1
low-latency-app  | main constructed elem:0,0,0 lfq-size:1
low-latency-app  | main constructed elem:1,10,100 lfq-size:2
low-latency-app  | main constructed elem:2,20,200 lfq-size:3
low-latency-app  | main constructed elem:3,30,300 lfq-size:4
low-latency-app  | consumeFunction read elem:0,0,0 lfq-size:3
low-latency-app  | main constructed elem:4,40,400 lfq-size:4
low-latency-app  | consumeFunction read elem:1,10,100 lfq-size:3
low-latency-app  | main constructed elem:5,50,500 lfq-size:4
```

In the last few output lines from `lf_queue_example.cpp` we can also see how the code finishes reading the last four elements in `store_`:

```
low-latency-app  | consumeFunction read elem:46,460,4600 lfq-size:3
low-latency-app  | consumeFunction read elem:47,470,4700 lfq-size:2
low-latency-app  | consumeFunction read elem:48,480,4800 lfq-size:1
low-latency-app  | consumeFunction read elem:49,490,4900 lfq-size:0
low-latency-app  | consumeFunction exiting.
low-latency-app  | main exiting.
```
