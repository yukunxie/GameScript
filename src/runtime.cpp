#include "gs/runtime.hpp"
#include "gs/global.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

namespace gs {

namespace {

std::string readFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return false;
    }
    output << content;
    return static_cast<bool>(output);
}

std::string resolveSourcePath(const std::string& path, const std::vector<std::string>& searchPaths) {
    namespace fs = std::filesystem;

    fs::path input(path);
    if (fs::exists(input)) {
        return input.string();
    }

    for (const auto& base : searchPaths) {
        fs::path candidate = fs::path(base) / input;
        if (fs::exists(candidate)) {
            return candidate.string();
        }
    }

    return {};
}

} // namespace

Runtime::Runtime()
    : module_(std::make_shared<Module>()),
      pool_(std::max<std::size_t>(2, std::thread::hardware_concurrency())),
            tasks_(pool_) {
        bindGlobalModule(hosts_);
}

HostRegistry& Runtime::host() {
    return hosts_;
}

bool Runtime::loadSourceFile(const std::string& path, const std::vector<std::string>& searchPaths) {
    const auto resolvedPath = resolveSourcePath(path, searchPaths);
    if (resolvedPath.empty()) {
        return false;
    }

    std::vector<std::string> importSearchPaths = searchPaths;
    importSearchPaths.push_back(std::filesystem::path(resolvedPath).parent_path().string());

    Module compiled;
    try {
        compiled = compileSourceFile(resolvedPath, importSearchPaths);
    } catch (...) {
        return false;
    }

    auto newModule = std::make_shared<Module>(std::move(compiled));
    {
        std::scoped_lock lock(moduleMutex_);
        module_ = std::move(newModule);
    }
    return true;
}

bool Runtime::loadBytecodeFile(const std::string& path) {
    const auto bc = readFile(path);
    if (bc.empty()) {
        return false;
    }

    auto newModule = std::make_shared<Module>(deserializeModuleText(bc));
    {
        std::scoped_lock lock(moduleMutex_);
        module_ = std::move(newModule);
    }
    return true;
}

bool Runtime::hotReloadSource(const std::string& path) {
    return loadSourceFile(path);
}

Value Runtime::call(const std::string& functionName, const std::vector<Value>& args) {
    std::shared_ptr<Module> snapshot;
    {
        std::scoped_lock lock(moduleMutex_);
        snapshot = module_;
    }
    VirtualMachine vm(snapshot, hosts_, tasks_);
    return vm.runFunction(functionName, args);
}

bool Runtime::saveBytecode(const std::string& path) const {
    std::shared_ptr<Module> snapshot;
    {
        std::scoped_lock lock(moduleMutex_);
        snapshot = module_;
    }
    return writeFile(path, serializeModuleText(*snapshot));
}

} // namespace gs
