#include "gs/task_system.hpp"

#include <stdexcept>

namespace gs {

TaskSystem::TaskSystem(ThreadPool& pool) : pool_(pool) {}

std::int64_t TaskSystem::enqueue(std::function<Value()> task) {
    auto future = pool_.submit([task = std::move(task)]() mutable { return task(); });
    std::scoped_lock lock(mutex_);
    const auto id = nextId_++;
    tasks_.emplace(id, std::move(future));
    return id;
}

Value TaskSystem::await(std::int64_t handle) {
    std::future<Value> future;
    {
        std::scoped_lock lock(mutex_);
        auto it = tasks_.find(handle);
        if (it == tasks_.end()) {
            throw std::runtime_error("Task handle not found");
        }
        future = std::move(it->second);
        tasks_.erase(it);
    }
    return future.get();
}

} // namespace gs
