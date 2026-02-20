#include "gs/type_system/class_type.hpp"

#include <stdexcept>

namespace gs {

ClassObject::ClassObject(const Type& typeRef,
                         std::string className,
                         std::size_t classIndex,
                         std::shared_ptr<const Module> modulePin)
    : type_(&typeRef),
      className_(std::move(className)),
      classIndex_(classIndex),
      modulePin_(std::move(modulePin)) {}

const Type& ClassObject::getType() const {
    return *type_;
}

const std::string& ClassObject::className() const {
    return className_;
}

std::size_t ClassObject::classIndex() const {
    return classIndex_;
}

const std::shared_ptr<const Module>& ClassObject::modulePin() const {
    return modulePin_;
}

ClassType::ClassType() {
    registerMethodAttribute("__str__", 0, [this](Object& self,
                                                  const std::vector<Value>& args,
                                                  const StringFactory& makeString,
                                                  const ValueStrInvoker& valueStr) {
        (void)args;
        return makeString(__str__(self, valueStr));
    });
}

const char* ClassType::name() const {
    return "Class";
}

std::string ClassType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto* cls = dynamic_cast<ClassObject*>(&self);
    if (!cls) {
        throw std::runtime_error("ClassType called with non-class object");
    }

    (void)valueStr;
    return "[Class " + cls->className() + "]";
}

} // namespace gs
