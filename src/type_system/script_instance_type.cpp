#include "gs/type_system/script_instance_type.hpp"

#include <stdexcept>

namespace gs {

ScriptInstanceObject::ScriptInstanceObject(const Type& typeRef,
                                           std::size_t classIndex,
                                           std::string className,
                                           std::shared_ptr<const Module> modulePin)
    : type_(&typeRef),
      classIndex_(classIndex),
      className_(std::move(className)),
      modulePin_(std::move(modulePin)) {}

const Type& ScriptInstanceObject::getType() const {
    return *type_;
}

std::size_t ScriptInstanceObject::classIndex() const {
    return classIndex_;
}

const std::string& ScriptInstanceObject::className() const {
    return className_;
}

const std::shared_ptr<const Module>& ScriptInstanceObject::modulePin() const {
    return modulePin_;
}

std::unordered_map<std::string, Value>& ScriptInstanceObject::fields() {
    return fields_;
}

const std::unordered_map<std::string, Value>& ScriptInstanceObject::fields() const {
    return fields_;
}

const char* ScriptInstanceType::name() const {
    return "Instance";
}

ScriptInstanceType::ScriptInstanceType() {
    registerMethodAttribute("__str__", 0, [this](Object& self,
                                                  const std::vector<Value>& args,
                                                  const StringFactory& makeString,
                                                  const ValueStrInvoker& valueStr) {
        (void)args;
        return makeString(__str__(self, valueStr));
    });
}

Value ScriptInstanceType::callMethod(Object& self,
                                     const std::string& method,
                                     const std::vector<Value>& args,
                                     const StringFactory& makeString,
                                     const ValueStrInvoker& valueStr) const {
    auto* instance = dynamic_cast<ScriptInstanceObject*>(&self);
    if (!instance) {
        throw std::runtime_error("ScriptInstanceType called with non-instance object");
    }

    (void)instance;
    return Type::callMethod(self, method, args, makeString, valueStr);
}

std::string ScriptInstanceType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto* instance = dynamic_cast<ScriptInstanceObject*>(&self);
    if (!instance) {
        throw std::runtime_error("ScriptInstanceType called with non-instance object");
    }

    (void)valueStr;
    return instance->className() + "#" + std::to_string(instance->objectId());
}

} // namespace gs
