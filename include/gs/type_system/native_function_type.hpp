#pragma once

#include "gs/type_system/type_base.hpp"
#include "gs/bytecode.hpp"

#include <functional>
#include <string>
#include <vector>

namespace gs {

// Forward declarations from binding.hpp
class HostContext;
using HostFunction = std::function<Value(HostContext& context, const std::vector<Value>&)>;

class NativeFunctionObject : public Object {
public:
    NativeFunctionObject(const Type& typeRef,
                         std::string functionName,
                         HostFunction callback);

    const Type& getType() const override;
    const std::string& functionName() const;
    Value invoke(HostContext& context, const std::vector<Value>& args) const;

private:
    const Type* type_;
    std::string functionName_;
    HostFunction callback_;
};

class NativeFunctionType : public Type {
public:
    NativeFunctionType();

    const char* name() const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;

private:
    static NativeFunctionObject& requireNativeFunction(Object& self);
};

} // namespace gs
