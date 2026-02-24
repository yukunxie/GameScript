#pragma once

#include "gs/type_system/script_callable_object.hpp"

#include <vector>

namespace gs {

class LambdaObject : public ScriptCallableObjectBase {
public:
    LambdaObject(const Type& typeRef,
                 std::size_t functionIndex,
                 std::shared_ptr<const Module> modulePin = nullptr,
                 std::vector<Value> captures = {});

    const std::vector<Value>& captures() const;

private:
    std::vector<Value> captures_;
};

class LambdaType : public Type {
public:
    LambdaType();
    const char* name() const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
};

} // namespace gs
