#include "gs/type_system/module_type.hpp"

#include <stdexcept>

namespace gs {

ModuleObject::ModuleObject(const Type& typeRef, std::string moduleName, std::shared_ptr<const Module> modulePin)
    : type_(&typeRef), moduleName_(std::move(moduleName)), modulePin_(std::move(modulePin)) {}

const Type& ModuleObject::getType() const {
    return *type_;
}

const std::string& ModuleObject::moduleName() const {
    return moduleName_;
}

const std::shared_ptr<const Module>& ModuleObject::modulePin() const {
    return modulePin_;
}

void ModuleObject::setModulePin(std::shared_ptr<const Module> modulePin) {
    modulePin_ = std::move(modulePin);
}

std::unordered_map<std::string, Value>& ModuleObject::exports() {
    return exports_;
}

const std::unordered_map<std::string, Value>& ModuleObject::exports() const {
    return exports_;
}

ModuleType::ModuleType() = default;

const char* ModuleType::name() const {
    return "Module";
}

Value ModuleType::getMember(Object& self, const std::string& member) const {
    auto& module = requireModule(self);
    auto it = module.exports().find(member);
    if (it != module.exports().end()) {
        return it->second;
    }
    return Type::getMember(self, member);
}

Value ModuleType::setMember(Object& self, const std::string& member, const Value& value) const {
    auto& module = requireModule(self);
    module.exports()[member] = value;
    return value;
}

std::string ModuleType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto& module = requireModule(self);
    (void)valueStr;
    return "Module(" + module.moduleName() + ")#" + std::to_string(module.objectId());
}

ModuleObject& ModuleType::requireModule(Object& self) {
    auto* module = dynamic_cast<ModuleObject*>(&self);
    if (!module) {
        throw std::runtime_error("ModuleType called with non-module object");
    }
    return *module;
}

} // namespace gs
