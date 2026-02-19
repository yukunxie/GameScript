#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace gs {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t workers = std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F>
    auto submit(F&& fn) -> std::future<decltype(fn())> {
        using ReturnType = decltype(fn());
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(fn));
        std::future<ReturnType> future = task->get_future();
        {
            std::scoped_lock lock(mutex_);
            jobs_.push([task]() { (*task)(); });
        }
        cv_.notify_one();
        return future;
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> jobs_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_{false};
};

} // namespace gs
