#pragma once

#include "gs/type_system/type_base.hpp"

#include <memory>

namespace gs {

class ScriptCallableObjectBase : public Object {
public:
    ScriptCallableObjectBase(const Type& typeRef,
                             std::size_t functionIndex,
                             std::shared_ptr<const Module> modulePin = nullptr)
        : type_(&typeRef),
          functionIndex_(functionIndex),
          modulePin_(std::move(modulePin)) {}

    const Type& getType() const override {
        return *type_;
    }

    std::size_t functionIndex() const {
        return functionIndex_;
    }

    const std::shared_ptr<const Module>& modulePin() const {
        return modulePin_;
    }

private:
    const Type* type_;
    std::size_t functionIndex_{0};
    std::shared_ptr<const Module> modulePin_;
};

} // namespace gs
