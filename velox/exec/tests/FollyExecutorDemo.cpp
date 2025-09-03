#include <folly/init/Init.h>
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
      for (int i = 0; i < 20; ++i) { // 2秒计算
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

int main(int argc, char* argv[]) {
  folly::Init init{&argc, &argv, false};

  // Initializes the process-wide memory-manager with the default options.
  memory::initializeMemoryManager({});

  FollyExecutorDemo demo;
  demo.firstDemo();

  demo.executorExceptionWillNotCrash(false);
  demo.executorExceptionWillNotCrash(true);

  DemoOperator o;
  o.runExecutor();
  std::this_thread::sleep_for(std::chrono::seconds(10));
  o.stop();

  return 0;
}
