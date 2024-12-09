# Logger
The `Logger` class works mainly through a `LFQueue` instance of type `LogElement` called `queue_`. 

A `LogElement` is a struct consisting of two things:
- a `LogType`, `type_`, which defaults to CHAR, but which can be INTEGER, LOG_INTEGER, etc.; and
- a `union` for char, int, long, etc. types.

For instance, we can create a `LogElement` which initializes a char with some given `value`:
```
auto le = LogElement{LogType::CHAR, {.c = value}}
```

The point of this struct is that we can hold a variable of any of the types in the union (any of the `LogType`) and also access to its type. For example, if we want to know the variable type from the `LogElement` (`le`) above, we write `le->type_`; and to access the variable value for the `LogElement` (`le`) above, we write `le->u_.c` - as it is a char; if instead one typed `le->u_.i`, they should find it equal to the default value zero.

The `LFQueue` `queue_` has a default size `LOG_QUEUE_SIZE`. Recall this is the size of the queue, not the number of elements in the queue, which can instead be accessed through `queue_.size()`. 

Now, the `Logger` constructor takes a std::string `file_name`, which gets assigned to `file_name_` and is used to open the ofstream `file_`. To write that log file, the constructor creates and starts a thread `logger_thread_`, which is closed only when the destructor for `Logger` gets called (that is, when the `Logger` instance gets out of scope). On that `logger_thread_`, the process `flushQueue` is called. In `flushQueue`, we have a for-loop (looping over all `LogElement`s in `queue_`) inside a while loop, which keeps active until the std::atomic variable `running_` is set to false. That only happens when the destructor for `Logger` gets called. This means that, even when we finish looping over all elements in `queue_`, if new elements are added into it while the `Logger` instance is still around, then they will immediately be added onto `file_`. 

How we add new data into the `queue_` is through the `log` function. This function calls `pushValue` for each character in the input char (warping each value into a `LogElement` and pushing it into `queue_`). If a character `%` is found, this is substituted with its respective value given in `value` or `args` (`value` simply refers to the first `%` instance in the input char; while `args` refers to the second, third, fourth, etc. `%` instances in the input char). For example, in  `logging_example.cpp`:
```
Logger logger("/usr/outputs/logging_example.log");

logger.log("Logging a char:% an int:% and an unsigned:%\n", c, i, ul);
```
- A `Logger` instance `logger` gets created.
- "/usr/outputs/logging_example.log" gets assigned to the `logger`'s `file_name_`. Then we initialise the `logger`'s ofstream `file_` using `file_name_`. 
- We call the `logger`'s member function `log` with the `const char *s` "Logging a char:% an int:% and an unsigned:%\n": the first `%` will correspond to `c` (the `const T &value` in the `log` function declaration); the second `%` will correspond to `i`, and the third `%` will correspond to `ul` (both in `A... args` in the `log` function declaration).