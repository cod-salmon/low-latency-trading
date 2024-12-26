#include "thread_utils.h"
#include "lf_queue.h"

struct MyStruct {
  int d_[3];
};

using namespace Common;

auto consumeFunction(LFQueue<MyStruct>* lfq) {
  using namespace std::literals::chrono_literals;
  std::this_thread::sleep_for(5s);

  while(lfq->size()) {
    const auto d = lfq->getNextToRead();
    lfq->updateReadIndex();

    std::cout << "consumeFunction read elem:" << d->d_[0] << "," << d->d_[1] << "," << d->d_[2] << " lfq-size:" << lfq->size() << std::endl;

    std::this_thread::sleep_for(1s);
  }

  std::cout << "consumeFunction exiting." << std::endl;
}

int main(int, char **) {
  // Create lock free queue of size 20
  LFQueue<MyStruct> lfq(20);
  // Create thread at core ID -1. Launch thread with process consumeFunction, which takes a LFQueue argument. Attach thread process to std::thread object "ct".
  // consumeFunction reads out each element in the input LFQueue
  auto ct = createAndStartThread(-1, "", consumeFunction, &lfq);
  // while thread process assigned to "ct" is running, perform
  // the following...
  for(auto i = 0; i < 50; ++i) {
    // Create one MyStruct instance
    const MyStruct d{i, i * 10, i * 100};
    // Assign it to the appropiate element in the LFQueue
    *(lfq.getNextToWriteTo()) = d;
    // Update index to write
    lfq.updateWriteIndex();
    // Inform of the new MyStruct we've just written
    std::cout << "main constructed elem:" << d.d_[0] << "," << d.d_[1] << "," << d.d_[2] << " lfq-size:" << lfq.size() << std::endl;
    // Simulate some harded work
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);
  }

  // Call ct destructor when thread process ends and not before.
  ct->join();
  // Note in this case, we start writing onto the LFQueue before
  // the thread has time to start reading out the LFQueue.
  // As the LFQueue gets written, a bit after, is being read by thread.
  std::cout << "main exiting." << std::endl;

  return 0;
}
