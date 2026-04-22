#include <mosqueeze/bench/ThreadPool.hpp>

#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
#include <vector>

int main() {
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
    std::cout << "[PASS] ThreadPool_test\n";
    return 0;
}
