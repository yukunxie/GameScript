#include "gs/global.hpp"
#include "gs/compiler.hpp"
#include "gs/type_system.hpp"

#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
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

std::string formatAssertMessage(HostContext& context,
                                const std::string& format,
                                const std::vector<Value>& args,
                                std::size_t argStart) {
    std::ostringstream out;
    std::size_t i = 0;
    std::size_t argIndex = argStart;
    while (i < format.size()) {
        if (format[i] == '{' && i + 1 < format.size() && format[i + 1] == '}') {
            if (argIndex < args.size()) {
                out << context.__str__(args[argIndex++]);
            } else {
                out << "{}";
            }
            i += 2;
            continue;
        }
        out << format[i];
        ++i;
    }

    while (argIndex < args.size()) {
        if (out.tellp() > 0) {
            out << ' ';
        }
        out << context.__str__(args[argIndex++]);
    }
    return out.str();
}

} // namespace

void bindGlobalModule(HostRegistry& host) {
    host.bind("loadModule", [&host](HostContext& context, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("loadModule() accepts at least one argument");
        }
        const std::string moduleName = context.__str__(args[0]);

        for (std::size_t i = 1; i < args.size(); ++i) {
            (void)context.__str__(args[i]);
        }

        static ModuleType moduleType;
        static NativeFunctionType nativeFunctionType;

        if (host.hasModule(moduleName)) {
            const std::string builtinKey = "builtin:" + moduleName;
            Value cached = Value::Nil();
            if (context.tryGetCachedModuleObject(builtinKey, cached)) {
                context.ensureModuleInitialized(cached);
                return cached;
            }

            Value moduleRef = host.resolveBuiltin(moduleName,
                                                  context,
                                                  nativeFunctionType,
                                                  moduleType);
            context.cacheModuleObject(builtinKey, moduleRef);
            context.ensureModuleInitialized(moduleRef);
            return moduleRef;
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

        const std::string moduleKey = "file:" + modulePath;
        Value cached = Value::Nil();
        if (context.tryGetCachedModuleObject(moduleKey, cached)) {
            context.ensureModuleInitialized(cached);
            return cached;
        }

        Value moduleRef = context.createObject(std::make_unique<ModuleObject>(moduleType, moduleName, it->second));
        context.cacheModuleObject(moduleKey, moduleRef);
        context.ensureModuleInitialized(moduleRef);
        return moduleRef;
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

    host.bind("assert", [](HostContext& context, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("assert(condition, format, args...) requires at least condition");
        }

        const bool condition = args[0].asInt() != 0;
        if (condition) {
            return Value::Int(1);
        }

        std::string message = "assert failed";
        if (args.size() >= 2) {
            message = formatAssertMessage(context,
                                          context.__str__(args[1]),
                                          args,
                                          2);
        }
        throw std::runtime_error("assertion failed: " + message);
    });
}

} // namespace gs
