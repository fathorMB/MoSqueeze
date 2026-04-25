#pragma once

#include <atomic>
#include <csignal>

namespace mosqueeze::bench {

class SignalHandler {
public:
    static bool interrupted() {
        return interrupted_.load(std::memory_order_acquire);
    }

    static int receivedSignal() {
        return receivedSignal_.load(std::memory_order_acquire);
    }

    static void install() {
        if (installed_.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        std::signal(SIGINT, &SignalHandler::handle);
        std::signal(SIGTERM, &SignalHandler::handle);
    }

private:
    static void handle(int signo) {
        interrupted_.store(true, std::memory_order_release);
        receivedSignal_.store(signo, std::memory_order_release);
    }

    static inline std::atomic<bool> interrupted_{false};
    static inline std::atomic<int> receivedSignal_{0};
    static inline std::atomic<bool> installed_{false};
};

} // namespace mosqueeze::bench