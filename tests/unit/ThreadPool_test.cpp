#include <mosqueeze/bench/ThreadPool.hpp>

#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    {
        mosqueeze::bench::ThreadPool pool(4);
        pool.waitAll();
    }

    {
        mosqueeze::bench::ThreadPool pool(4);

        std::atomic<int> counter{0};
        std::vector<std::future<void>> futures;
        futures.reserve(100);

        for (int i = 0; i < 100; ++i) {
            futures.push_back(pool.submit([&counter] {
                ++counter;
            }));
        }

        pool.waitAll();
        for (auto& future : futures) {
            future.get();
        }

        assert(counter == 100);
    }

    {
        // Deadlock regression: immediate wait after burst submit.
        mosqueeze::bench::ThreadPool pool(8);
        std::atomic<int> counter{0};
        std::vector<std::future<void>> futures;
        futures.reserve(50);

        for (int i = 0; i < 50; ++i) {
            futures.push_back(pool.submit([&counter] {
                ++counter;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }));
        }

        pool.waitAll();
        for (auto& future : futures) {
            future.get();
        }
        assert(counter == 50);
    }

    std::cout << "[PASS] ThreadPool_test\n";
    return 0;
}
