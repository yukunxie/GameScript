#pragma once

#include "gs/type_system/script_callable_object.hpp"

namespace gs {

class FunctionObject : public ScriptCallableObjectBase {
public:
    FunctionObject(const Type& typeRef,
                   std::size_t functionIndex,
                   std::shared_ptr<const Module> modulePin = nullptr);
};

class FunctionType : public Type {
public:
    FunctionType();
    const char* name() const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
};

} // namespace gs
