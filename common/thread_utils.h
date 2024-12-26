#pragma once

#include <iostream>
#include <atomic>
#include <thread>
#include <unistd.h>

#include <sys/syscall.h>

namespace Common {
  // Set affinity for current thread to be pinned to the provided core_id CPU
  inline auto setThreadCore(int core_id) noexcept {
    // Creates a CPU set from the input core_id CPU
    cpu_set_t cpuset; 
    CPU_ZERO(&cpuset); 
    CPU_SET(core_id, &cpuset); 
    // The pthread_setaffinity_np() function sets the CPU affinity mask
    //  of the thread pthread_self() to the CPU set pointed to by "cpuset".
    //  If the call is successful, and the thread is not currently running on
    //  one of the CPUs in cpuset, then it is migrated to one of those CPUs.
    return (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0);
  }

  // Creates a thread instance, sets affinity on it, and passes the function 
  //    to be run on that thread as well as the arguments to the function.
  template<typename T, typename... A>
  inline auto createAndStartThread(int core_id, const std::string &name, T &&func, A &&... args) noexcept {
    // Instantiates a "thread" object with the func(args) process.
    auto t = new std::thread([&]() {
      if (core_id >= 0 && !setThreadCore(core_id)) {
        std::cerr << "Failed to set core affinity for " << name << " " << pthread_self() << " to " << core_id << std::endl;
        exit(EXIT_FAILURE);
      }
      std::cout << "Set core affinity for " << name << " " << pthread_self() << " to " << core_id << std::endl;
      // With std::forward you pass "func" and "args" as their original categories (that is, lvalue and rvalue)
      std::forward<T>(func)((std::forward<A>(args))...);
    });

    // Simulate some important work done by the thread by increasing time
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);

    return t;
  }
}
