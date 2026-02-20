#pragma once

#include "gs/type_system/type_base.hpp"

#include <memory>

namespace gs {

class ModuleObject : public Object {
public:
    ModuleObject(const Type& typeRef, std::string moduleName, std::shared_ptr<const Module> modulePin = nullptr);

    const Type& getType() const override;
    const std::string& moduleName() const;
    const std::shared_ptr<const Module>& modulePin() const;
    void setModulePin(std::shared_ptr<const Module> modulePin);
    std::unordered_map<std::string, Value>& exports();
    const std::unordered_map<std::string, Value>& exports() const;

private:
    const Type* type_;
    std::string moduleName_;
    std::shared_ptr<const Module> modulePin_;
    std::unordered_map<std::string, Value> exports_;
};

class ModuleType : public Type {
public:
    ModuleType();
    const char* name() const override;
    Value getMember(Object& self, const std::string& member) const override;
    Value setMember(Object& self, const std::string& member, const Value& value) const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;

private:
    static ModuleObject& requireModule(Object& self);
};

} // namespace gs
