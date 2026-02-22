#include "gs/binding.hpp"
#include "gs/type_system.hpp"

#include <stdexcept>

namespace gs {

HostRegistry::HostRegistry() {
    defineModule("system");
    defineModule("os");
    defineModule("math");
    defineModule("regex");
    defineModule("network");

    bindModuleFunction("system",
                       "gc",
                       [](HostContext& context, const std::vector<Value>& args) -> Value {
                           const std::int64_t generation = args.empty() ? 1 : args[0].asInt();
                           if (args.size() > 1) {
                               throw std::runtime_error("system.gc() accepts zero or one argument");
                           }
                           return context.collectGarbage(generation);
                       });
}

void HostRegistry::bind(const std::string& name, HostFunction fn) {
    if (!fn) {
        throw std::runtime_error("Host function callback is empty: " + name);
    }

    BuiltinEntry entry;
    entry.kind = BuiltinEntry::Kind::Function;
    entry.function = std::move(fn);
    builtins_[name] = std::move(entry);
}

void HostRegistry::defineModule(const std::string& moduleName) {
    auto it = builtins_.find(moduleName);
    if (it != builtins_.end()) {
        if (it->second.kind != BuiltinEntry::Kind::Module) {
            throw std::runtime_error("Builtin name already used by function: " + moduleName);
        }
        return;
    }

    BuiltinEntry entry;
    entry.kind = BuiltinEntry::Kind::Module;
    builtins_[moduleName] = std::move(entry);
}

void HostRegistry::bindModuleFunction(const std::string& moduleName,
                                      const std::string& exportName,
                                      HostFunction fn) {
    if (!fn) {
        throw std::runtime_error("Module function callback is empty: " + moduleName + "." + exportName);
    }

    defineModule(moduleName);
    auto& entry = builtins_.at(moduleName);
    if (entry.kind != BuiltinEntry::Kind::Module) {
        throw std::runtime_error("Builtin is not a module: " + moduleName);
    }

    entry.moduleFunctions[exportName] = std::move(fn);
}

bool HostRegistry::has(const std::string& name) const {
    return builtins_.contains(name);
}

bool HostRegistry::hasModule(const std::string& name) const {
    auto it = builtins_.find(name);
    if (it == builtins_.end()) {
        return false;
    }
    return it->second.kind == BuiltinEntry::Kind::Module;
}

Value HostRegistry::invoke(const std::string& name, HostContext& context, const std::vector<Value>& args) const {
    auto it = builtins_.find(name);
    if (it == builtins_.end()) {
        throw std::runtime_error("Host function not found: " + name);
    }

    if (it->second.kind != BuiltinEntry::Kind::Function) {
        throw std::runtime_error("Builtin is not a function: " + name);
    }

    return it->second.function(context, args);
}

Value HostRegistry::resolveBuiltin(const std::string& name,
                                   HostContext& context,
                                   NativeFunctionType& nativeFunctionType,
                                   ModuleType& moduleType) const {
    auto it = builtins_.find(name);
    if (it == builtins_.end()) {
        throw std::runtime_error("Builtin not found: " + name);
    }

    const auto& entry = it->second;
    if (entry.kind == BuiltinEntry::Kind::Function) {
        return context.createObject(std::make_unique<NativeFunctionObject>(nativeFunctionType,
                                                                            name,
                                                                            entry.function));
    }

    auto moduleObject = std::make_unique<ModuleObject>(moduleType, name);
    moduleObject->exports()["__name__"] = context.createString(name);
    for (const auto& [exportName, callback] : entry.moduleFunctions) {
        moduleObject->exports()[exportName] = context.createObject(std::make_unique<NativeFunctionObject>(nativeFunctionType,
                                                                                                             exportName,
                                                                                                             callback));
    }
    return context.createObject(std::move(moduleObject));
}

} // namespace gs
