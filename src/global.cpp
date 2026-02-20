#include "gs/global.hpp"
#include "gs/compiler.hpp"
#include "gs/type_system.hpp"

#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace gs {

namespace {

std::string resolveModulePath(const std::string& moduleSpec) {
    namespace fs = std::filesystem;
    std::string normalized = moduleSpec;
    if (normalized.find('/') == std::string::npos && normalized.find('\\') == std::string::npos) {
        for (char& c : normalized) {
            if (c == '.') {
                c = '/';
            }
        }
    }

    std::vector<std::string> candidates;
    candidates.push_back(normalized);
    if (normalized.size() < 3 || normalized.substr(normalized.size() - 3) != ".gs") {
        candidates.push_back(normalized + ".gs");
    }

    const std::vector<fs::path> roots = {
        fs::current_path(),
        fs::current_path() / "scripts",
        fs::current_path().parent_path() / "scripts"
    };

    for (const auto& candidate : candidates) {
        for (const auto& root : roots) {
            fs::path path = root / candidate;
            if (fs::exists(path)) {
                return fs::weakly_canonical(path).string();
            }
        }
    }

    return {};
}

void printValues(HostContext& context,
                 const std::vector<Value>& args,
                 bool withPrefix,
                 bool withTrailingNewline,
                 const char* separator) {
    if (withPrefix) {
        std::cout << "[script]";
    }
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i == 0) {
            std::cout << (withPrefix ? " " : "");
        } else {
            std::cout << separator;
        }
        std::cout << context.__str__(args[i]);
    }
    if (withTrailingNewline) {
        std::cout << '\n';
    }
}

} // namespace

void bindGlobalModule(HostRegistry& host) {
    host.bind("Module", [](HostContext& context, const std::vector<Value>& args) -> Value {
        if (args.size() > 1) {
            throw std::runtime_error("Module() accepts 0 or 1 argument");
        }
        std::string moduleName = "module";
        if (args.size() == 1) {
            moduleName = context.__str__(args[0]);
        }

        const std::string modulePath = resolveModulePath(moduleName);
        if (modulePath.empty()) {
            throw std::runtime_error("Module not found: " + moduleName);
        }

        static std::unordered_map<std::string, std::shared_ptr<Module>> moduleCache;
        auto it = moduleCache.find(modulePath);
        if (it == moduleCache.end()) {
            auto compiled = std::make_shared<Module>(compileSourceFile(modulePath));
            it = moduleCache.emplace(modulePath, std::move(compiled)).first;
        }

        static ModuleType moduleType;
        return context.createObject(std::make_unique<ModuleObject>(moduleType, moduleName, it->second));
    });

    host.bind("print", [](HostContext& context, const std::vector<Value>& args) -> Value {
        printValues(context, args, true, true, ", ");
        return Value::Int(0);
    });

    host.bind("printf", [](HostContext& context, const std::vector<Value>& args) -> Value {
        printValues(context, args, false, false, "");
        return Value::Int(0);
    });

    host.bind("str", [](HostContext& context, const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            throw std::runtime_error("str() requires exactly one argument");
        }
        return context.createString(context.__str__(args[0]));
    });

    host.bind("type", [](HostContext& context, const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            throw std::runtime_error("type() requires exactly one argument");
        }
        return context.createString(context.typeName(args[0]));
    });

    host.bind("id", [](HostContext& context, const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            throw std::runtime_error("id() requires exactly one argument");
        }
        const std::uint64_t idValue = context.objectId(args[0]);
        constexpr std::uint64_t kMaxValue = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        if (idValue > kMaxValue) {
            throw std::runtime_error("id() overflow");
        }
        return Value::Int(static_cast<std::int64_t>(idValue));
    });
}

} // namespace gs
