#include <folly/init/Init.h>

#include "folly/executors/IOThreadPoolExecutor.h"
#include "folly/futures/SharedPromise.h"
#include "velox/common/memory/Memory.h"
#include "velox/vector/tests/utils/VectorTestBase.h"

using namespace facebook::velox;

class FollyExecutorDemo : public test::VectorTestBase {
 private:
 public:
  int firstDemo() {
    // 1. Create a CPUThreadPoolExecutor with 2 threads
    auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(2);

    // 2. Submit an asynchronous task
    auto fut = via(executor.get(), [] {
      std::cout << "Hello from folly executor thread: "
                << std::this_thread::get_id() << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(10));
      return 42;
    });

    // 3. Chained thenValue, continues to run in the executor
    auto fut2 = std::move(fut).thenValue([executor](int value) {
      std::cout << "Received value: " << value << " in thread "
                << std::this_thread::get_id() << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(10));
      return value + 1;
    });

    // 4. Wait for the result
    int final = std::move(fut2).get();
    std::cout << "Final value: " << final << std::endl;

    // Shutdown the thread pool (it's automatically shutdown in destructor, this
    // is just demonstration)
    executor->join();
    return 0;
  }

  void executorExceptionWillNotCrash(bool getFuture) {
    auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(2);
    auto fut = via(executor.get(), [] {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      throw std::runtime_error("test error");
      return 42;
    });

    if (getFuture) {
      try {
        int final = std::move(fut).get();
        std::cout << "Final value: " << final << std::endl;
      } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
      }
    } else {
      fut = 0;
    }
    executor->join();
  }
};

class DemoOperator {
 public:
  DemoOperator()
      : executor_(std::make_shared<folly::CPUThreadPoolExecutor>(2)),
        canceled_(false) {}

  ~DemoOperator() {
    executor_->join();
  }

  void runExecutor() {
    auto fut = via(executor_.get(), [this]() {
      std::cout << "[executor] Task started in thread "
                << std::this_thread::get_id() << std::endl;
      for (int i = 0; i < 20; ++i) {
        if (canceled_.load()) {
          std::cout << "[executor] Task canceled, exiting early! thread "
                    << std::this_thread::get_id() << std::endl;
          return -1;
        }
        std::cout << "check " << i << " / 20" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
      }
      std::cout << "[executor] Task finished, thread "
                << std::this_thread::get_id() << std::endl;
      return 42;
    });
  }

  void stop() {
    canceled_.store(true);
  }

 private:
  std::shared_ptr<folly::CPUThreadPoolExecutor> executor_;
  std::atomic<bool> canceled_;
};

class Loader {
 public:
  Loader() : state_(State::kPlanned) {}
  enum class State { kPlanned, kLoading, kCancelled, kLoaded };

  bool loadOrFuture(folly::SemiFuture<bool>* wait) {
    {
      std::lock_guard<std::mutex> l(mutex_);
      if (state_ == State::kCancelled || state_ == State::kLoaded) {
        return true;
      }
      if (state_ == State::kLoading) {
        if (wait == nullptr) {
          return false;
        }
        if (promise_ == nullptr) {
          promise_ = std::make_unique<folly::SharedPromise<bool>>();
        }
        *wait = promise_->getSemiFuture();
        return false;
      }

      state_ = State::kLoading;
    }

    uint64_t s = 0;
    for (int i = 0; i < N; i++) {
      for (int j = 0; j < i; j++) {
        s += i * j;
      }
    }
    sum.store(s);
    setEndState(State::kLoaded);
    return true;
  }

  uint64_t getSum() const {
    return sum.load();
  }

 private:
  void setEndState(State endState) {
    std::unique_ptr<folly::SharedPromise<bool>> promise;
    {
      std::lock_guard<std::mutex> l(mutex_);
      state_ = endState;
      promise.swap(promise_);
    }
    if (promise != nullptr) {
      promise->setValue(true);
    }
  }

  static constexpr int N = 10000;
  State state_;
  std::unique_ptr<folly::SharedPromise<bool>> promise_;
  std::mutex mutex_;
  std::atomic<uint64_t> sum{0};
};

class ExecutorHolderDemo {
 public:
  ExecutorHolderDemo()
      : executor_(std::make_shared<folly::IOThreadPoolExecutor>(6)) {
    for (int i = 0; i < 800; i++) {
      loaders_.push_back(std::make_shared<Loader>());
    }
  }

  ~ExecutorHolderDemo() {
    for (auto& l : loaders_) {
      folly::SemiFuture<bool> waitFuture(false);
      if (!l->loadOrFuture(&waitFuture)) {
        waitFuture.wait();
      }
    }
    for (const auto& l : loaders_) {
      std::cout << l->getSum() << std::endl;
    }
  }

  void run() {
    for (auto& loader : loaders_) {
      executor_->add([loader]() -> void { loader->loadOrFuture(nullptr); });
    }
  }

 private:
  std::shared_ptr<folly::Executor> executor_;
  std::vector<std::shared_ptr<Loader>> loaders_;
};

void demo1() {
  FollyExecutorDemo demo;
  demo.firstDemo();
}

void demo2() {
  FollyExecutorDemo demo;
  demo.executorExceptionWillNotCrash(false);
  demo.executorExceptionWillNotCrash(true);
}

void demo3() {
  DemoOperator o;
  o.runExecutor();
  std::this_thread::sleep_for(std::chrono::seconds(10));
  o.stop();
}

void demo4() {
  ExecutorHolderDemo executor_holder_demo;
  executor_holder_demo.run();
}

int main(int argc, char* argv[]) {
  folly::Init init{&argc, &argv, false};

  // Initializes the process-wide memory-manager with the default options.
  memory::initializeMemoryManager({});

  if (argc != 2) {
    std::cout << "Usgae: ./demo demoName" << std::endl;
    return 1;
  }

  auto cmd = std::stoi(argv[1]);
  switch (cmd) {
    case 1:
      demo1();
      break;
    case 2:
      demo2();
      break;
    case 3:
      demo3();
      break;
    case 4:
      demo4();
      break;
    default:
      demo4();
      break;
  }
  return 0;
}
