#include "gs/type_system/lambda_type.hpp"

namespace gs {

LambdaObject::LambdaObject(const Type& typeRef,
                           std::size_t functionIndex,
                           std::shared_ptr<const Module> modulePin,
                           std::vector<Value> captures)
        : ScriptCallableObjectBase(typeRef, functionIndex, std::move(modulePin)),
          captures_(std::move(captures)) {}

const std::vector<Value>& LambdaObject::captures() const {
    return captures_;
}

const char* LambdaType::name() const {
    return "Lambda";
}

LambdaType::LambdaType() {
    registerMethodAttribute("__str__", 0, [this](Object& self,
                                                  const std::vector<Value>& args,
                                                  const StringFactory& makeString,
                                                  const ValueStrInvoker& valueStr) {
        (void)args;
        return makeString(__str__(self, valueStr));
    });
}

std::string LambdaType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    (void)self;
    (void)valueStr;
    return "[Lambda]";
}

} // namespace gs
