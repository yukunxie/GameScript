#pragma once

#include "gs/bytecode.hpp"

#include <memory>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gs {

class Object;
class NativeFunctionType;
class ModuleType;

class HostContext {
public:
    virtual ~HostContext() = default;
    virtual Value createObject(std::unique_ptr<Object> object) = 0;
    virtual Value createString(const std::string& text) = 0;
    virtual Object& getObject(const Value& ref) = 0;
    virtual std::string __str__(const Value& value) = 0;
    virtual std::string typeName(const Value& value) = 0;
    virtual std::uint64_t objectId(const Value& ref) = 0;
    virtual Value collectGarbage(std::int64_t generation) = 0;
    virtual void ensureModuleInitialized(const Value& moduleRef) = 0;
    virtual bool tryGetCachedModuleObject(const std::string& moduleKey, Value& outModuleRef) = 0;
    virtual void cacheModuleObject(const std::string& moduleKey, const Value& moduleRef) = 0;
};

using HostFunction = std::function<Value(HostContext& context, const std::vector<Value>&)>;

class HostRegistry {
public:
    HostRegistry();

    void bind(const std::string& name, HostFunction fn);
    void defineModule(const std::string& moduleName);
    void bindModuleFunction(const std::string& moduleName,
                            const std::string& exportName,
                            HostFunction fn);
    bool has(const std::string& name) const;
    bool hasModule(const std::string& name) const;
    Value invoke(const std::string& name, HostContext& context, const std::vector<Value>& args) const;
    Value resolveBuiltin(const std::string& name,
                         HostContext& context,
                         NativeFunctionType& nativeFunctionType,
                         ModuleType& moduleType) const;

private:
    struct BuiltinEntry {
        enum class Kind {
            Function,
            Module
        };

        Kind kind{Kind::Function};
        HostFunction function;
        std::unordered_map<std::string, HostFunction> moduleFunctions;
    };

    std::unordered_map<std::string, BuiltinEntry> builtins_;
};

} // namespace gs
