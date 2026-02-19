#pragma once

#include "gs/bytecode.hpp"
#include "gs/thread_pool.hpp"

#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <unordered_map>

namespace gs {

class TaskSystem {
public:
    explicit TaskSystem(ThreadPool& pool);

    std::int64_t enqueue(std::function<Value()> task);
    Value await(std::int64_t handle);

private:
    ThreadPool& pool_;
    std::mutex mutex_;
    std::int64_t nextId_{1};
    std::unordered_map<std::int64_t, std::future<Value>> tasks_;
};

} // namespace gs
