#pragma once

#include "gs/type_system/type_base.hpp"

#include <memory>

namespace gs {

class ScriptInstanceObject : public Object {
public:
    ScriptInstanceObject(const Type& typeRef,
                         std::size_t classIndex,
                         std::string className,
                         std::shared_ptr<const Module> modulePin = nullptr);

    const Type& getType() const override;
    std::size_t classIndex() const;
    const std::string& className() const;
    const std::shared_ptr<const Module>& modulePin() const;
    std::unordered_map<std::string, Value>& fields();
    const std::unordered_map<std::string, Value>& fields() const;

private:
    const Type* type_;
    std::size_t classIndex_{0};
    std::string className_;
    std::shared_ptr<const Module> modulePin_;
    std::unordered_map<std::string, Value> fields_;
};

class ScriptInstanceType : public Type {
public:
    ScriptInstanceType();
    const char* name() const override;
    Value callMethod(Object& self,
                     const std::string& method,
                     const std::vector<Value>& args,
                     const StringFactory& makeString,
                     const ValueStrInvoker& valueStr) const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
};

} // namespace gs
