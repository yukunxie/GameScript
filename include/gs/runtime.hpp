#pragma once

#include "gs/binding.hpp"
#include "gs/compiler.hpp"
#include "gs/task_system.hpp"
#include "gs/thread_pool.hpp"
#include "gs/vm.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace gs {

class Runtime {
public:
    Runtime();

    HostRegistry& host();

    bool loadSourceFile(const std::string& path, const std::vector<std::string>& searchPaths = {});
    bool loadBytecodeFile(const std::string& path);
    bool hotReloadSource(const std::string& path);
    const std::string& lastError() const;
    void setDumpTransformedSource(bool enabled);
    bool dumpTransformedSourceEnabled() const;

    Value call(const std::string& functionName, const std::vector<Value>& args = {});

    bool saveBytecode(const std::string& path) const;

private:
    mutable std::mutex moduleMutex_;
    std::shared_ptr<Module> module_;
    std::string lastError_;
    bool dumpTransformedSource_{true};
    HostRegistry hosts_;
    ThreadPool pool_;
    TaskSystem tasks_;
};

} // namespace gs
