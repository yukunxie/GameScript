#include "gs/type_system/function_type.hpp"

namespace gs {

FunctionObject::FunctionObject(const Type& typeRef,
                               std::size_t functionIndex,
                               std::shared_ptr<const Module> modulePin)
    : type_(&typeRef), functionIndex_(functionIndex), modulePin_(std::move(modulePin)) {}

const Type& FunctionObject::getType() const {
    return *type_;
}

std::size_t FunctionObject::functionIndex() const {
    return functionIndex_;
}

const std::shared_ptr<const Module>& FunctionObject::modulePin() const {
    return modulePin_;
}

const char* FunctionType::name() const {
    return "Function";
}

FunctionType::FunctionType() {
    registerMethodAttribute("__str__", 0, [this](Object& self,
                                                  const std::vector<Value>& args,
                                                  const StringFactory& makeString,
                                                  const ValueStrInvoker& valueStr) {
        (void)args;
        return makeString(__str__(self, valueStr));
    });
}

std::string FunctionType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    (void)self;
    (void)valueStr;
    return "[Function]";
}

} // namespace gs
