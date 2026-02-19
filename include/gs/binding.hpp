#pragma once

#include "gs/bytecode.hpp"

#include <memory>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gs {

class Object;

class HostContext {
public:
    virtual ~HostContext() = default;
    virtual Value createObject(std::unique_ptr<Object> object) = 0;
    virtual Object& getObject(const Value& ref) = 0;
};

using HostFunction = std::function<Value(HostContext& context, const std::vector<Value>&)>;

class HostRegistry {
public:
    void bind(const std::string& name, HostFunction fn);
    bool has(const std::string& name) const;
    Value invoke(const std::string& name, HostContext& context, const std::vector<Value>& args) const;

private:
    std::unordered_map<std::string, HostFunction> funcs_;
};

} // namespace gs
