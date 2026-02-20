#pragma once

#include "gs/type_system/type_base.hpp"

#include <memory>

namespace gs {

class ClassObject : public Object {
public:
    ClassObject(const Type& typeRef,
                std::string className,
                std::size_t classIndex,
                std::shared_ptr<const Module> modulePin);

    const Type& getType() const override;
    const std::string& className() const;
    std::size_t classIndex() const;
    const std::shared_ptr<const Module>& modulePin() const;

private:
    const Type* type_;
    std::string className_;
    std::size_t classIndex_{0};
    std::shared_ptr<const Module> modulePin_;
};

class ClassType : public Type {
public:
    ClassType();
    const char* name() const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
};

} // namespace gs
