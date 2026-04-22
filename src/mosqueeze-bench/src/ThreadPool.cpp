#include <mosqueeze/bench/ThreadPool.hpp>

#include <utility>

namespace mosqueeze::bench {

ThreadPool::ThreadPool(size_t threads) {
    const size_t workerCount = threads == 0 ? 1 : threads;
    workers_.reserve(workerCount);
    for (size_t i = 0; i < workerCount; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });
                    if (stop_ && tasks_.empty()) {
                        return;
                    }
                    task = std::move(tasks_.front());
                    tasks_.pop();
                    ++activeTasks_;
                }

                try {
                    task();
                } catch (...) {
                    // Exceptions are captured in futures by packaged_task.
                }

                {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    --activeTasks_;
                }
                completed_.notify_all();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    completed_.wait(lock, [this] {
        return tasks_.empty() && activeTasks_ == 0;
    });
}

} // namespace mosqueeze::bench
