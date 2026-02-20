#pragma once

#include "gs/type_system/type_base.hpp"

#include <memory>

namespace gs {

class FunctionObject : public Object {
public:
    FunctionObject(const Type& typeRef,
                   std::size_t functionIndex,
                   std::shared_ptr<const Module> modulePin = nullptr);

    const Type& getType() const override;
    std::size_t functionIndex() const;
    const std::shared_ptr<const Module>& modulePin() const;

private:
    const Type* type_;
    std::size_t functionIndex_{0};
    std::shared_ptr<const Module> modulePin_;
};

class FunctionType : public Type {
public:
    FunctionType();
    const char* name() const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
};

} // namespace gs
