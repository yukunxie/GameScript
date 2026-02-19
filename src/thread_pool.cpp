#include "gs/thread_pool.hpp"

#include <algorithm>

namespace gs {

ThreadPool::ThreadPool(std::size_t workers) {
    if (workers == 0) {
        workers = 2;
    }

    workers_.reserve(workers);
    for (std::size_t i = 0; i < workers; ++i) {
        workers_.emplace_back([this]() {
            for (;;) {
                std::function<void()> job;
                {
                    std::unique_lock lock(mutex_);
                    cv_.wait(lock, [this]() { return stop_ || !jobs_.empty(); });
                    if (stop_ && jobs_.empty()) {
                        return;
                    }
                    job = std::move(jobs_.front());
                    jobs_.pop();
                }
                job();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::scoped_lock lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

} // namespace gs
