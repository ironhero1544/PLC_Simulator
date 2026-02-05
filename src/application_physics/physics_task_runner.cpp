// physics_task_runner.cpp
//
// Shared task runner for per-component physics updates.

#include "plc_emulator/application_physics/physics_task_runner.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace plc {
namespace {

constexpr int kMaxWorkerThreads = 4;
constexpr bool kForceSerialComponentUpdates = true;

class ComponentTaskRunner {
 public:
  ComponentTaskRunner() {
    unsigned int hardware_threads = std::thread::hardware_concurrency();
    size_t desired = hardware_threads > 1 ? hardware_threads - 1 : 1;
    worker_count_ = std::min(static_cast<size_t>(kMaxWorkerThreads), desired);

    for (size_t i = 0; i < worker_count_; ++i) {
      workers_[i] = std::thread([this]() { WorkerLoop(); });
    }
  }

  ~ComponentTaskRunner() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      shutdown_ = true;
      has_task_ = true;
    }
    cv_.notify_all();
    for (size_t i = 0; i < worker_count_; ++i) {
      if (workers_[i].joinable()) {
        workers_[i].join();
      }
    }
  }

  void Run(size_t count, ComponentTaskFn fn, void* context) {
    if (count == 0) {
      return;
    }
    if (kForceSerialComponentUpdates || worker_count_ == 0 || count < 2) {
      for (size_t i = 0; i < count; ++i) {
        fn(i, context);
      }
      return;
    }

    {
      std::unique_lock<std::mutex> lock(mutex_);
      done_cv_.wait(lock, [this]() { return !has_task_; });
      task_fn_ = fn;
      task_context_ = context;
      task_count_ = count;
      next_index_.store(0);
      active_workers_ = worker_count_;
      has_task_ = true;
    }

    cv_.notify_all();

    std::unique_lock<std::mutex> lock(mutex_);
    done_cv_.wait(lock, [this]() { return !has_task_; });
  }

 private:
  void WorkerLoop() {
    for (;;) {
      ComponentTaskFn fn = nullptr;
      void* context = nullptr;
      size_t count = 0;

      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return has_task_; });
        if (shutdown_) {
          return;
        }
        fn = task_fn_;
        context = task_context_;
        count = task_count_;
      }

      for (;;) {
        size_t index = next_index_.fetch_add(1);
        if (index >= count) {
          break;
        }
        fn(index, context);
      }

      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_workers_ > 0) {
          active_workers_--;
        }
        if (active_workers_ == 0) {
          has_task_ = false;
          done_cv_.notify_one();
        }
      }
    }
  }

  std::array<std::thread, kMaxWorkerThreads> workers_;
  size_t worker_count_ = 0;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::condition_variable done_cv_;
  std::atomic<size_t> next_index_{0};
  ComponentTaskFn task_fn_ = nullptr;
  void* task_context_ = nullptr;
  size_t task_count_ = 0;
  size_t active_workers_ = 0;
  bool has_task_ = false;
  bool shutdown_ = false;
};

ComponentTaskRunner& GetComponentTaskRunner() {
  static ComponentTaskRunner runner;
  return runner;
}

}  // namespace

void RunComponentTasks(size_t count, ComponentTaskFn fn, void* context) {
  GetComponentTaskRunner().Run(count, fn, context);
}

}  // namespace plc
