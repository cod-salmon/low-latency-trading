# Threads
Let's say we wrote a function called `dummyFunction`, which takes two integers `a` and `b`and prints out their sum `a + b` (see `thread_example.cpp`).

Also, during `main` we have the following line of code:

```
auto t = createAndStartThread(1, "dummyFunction", dummyFunction, 15, 51, true);
```
## createAndStartThread
This function takes 
- an int type referring to the `core_id`, 
- a std::string, referring to the process or name of the thread;
- an r-value reference to a function `func`, returning `T`;
- an r-value reference to a set of parameters `args` of types encapsulated in `A`.

**Affinity** means that instead of being free to run the thread on any CPU it feels like, the OS scheduler is asked to only schedule a given thread to a single CPU or a pre-defined set of CPUs. 

During `createAndStartThread`, we instatiate a `std:.thread` object:
(1) by running `setThreadCore` with the input `core_id`. During `setThreadCore`, a cpuset gets created with the core_id CPU. Then `pthread_setaffinity_np` assigns an affinity of the thread to the cpuset consisting of the core_id CPU. If affinity was correctly set, then:
(2) we forward the arguments `args` to the function `func`. Note this is done through `std::forward`, which allows for **perfect forwarding** - i.e., the process of forwarding arguments to other functions in a way that respects their original value category - whether they are lvalues or rvalues. 

Note the process run inside the `thread` might finish before the destructor for `thread` gets called, or the process might have not yet finished before the destructor for `thread` is called. That is why often we have, during the lifetime of the `thread` instance, a call to `std::thread::join()`, which makes sure that the thread will be cleared once the process assigned to it is done; for instance:

```
auto t = createAndStartThread(1, "dummyFunction", dummyFunction, 15, 51, true);

std::cout << "Waiting for thread to be done." << std::endl;
  
t->join();

std::cout << "Exiting." << std::endl;
```

For the main in thread_example.cpp we get the following output:

```
low-latency-app  | Set core affinity for dummyFunction1 127451279656512 to -1
low-latency-app  | dummyFunction(12,21)
low-latency-app  | dummyFunction output=33
low-latency-app  | dummyFunction done.
low-latency-app  | Set core affinity for dummyFunction2 127451269170752 to 1
low-latency-app  | dummyFunction(15,51)
low-latency-app  | dummyFunction output=66
low-latency-app  | dummyFunction sleeping...
low-latency-app  | main waiting for threads to be done.
low-latency-app  | dummyFunction done.
low-latency-app  | main exiting.

```

