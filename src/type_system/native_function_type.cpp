#include "gs/type_system/native_function_type.hpp"

#include <stdexcept>

namespace gs {

NativeFunctionObject::NativeFunctionObject(const Type& typeRef,
                                           std::string functionName,
                                                                                     HostFunction callback)
    : type_(&typeRef),
      functionName_(std::move(functionName)),
            callback_(std::move(callback)) {}

const Type& NativeFunctionObject::getType() const {
    return *type_;
}

const std::string& NativeFunctionObject::functionName() const {
    return functionName_;
}

Value NativeFunctionObject::invoke(HostContext& context, const std::vector<Value>& args) const {
    if (!callback_) {
        throw std::runtime_error("Native function callback is not set: " + functionName_);
    }
    return callback_(context, args);
}

NativeFunctionType::NativeFunctionType() = default;

const char* NativeFunctionType::name() const {
    return "NativeFunction";
}

std::string NativeFunctionType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto& nativeFn = requireNativeFunction(self);
    (void)valueStr;
    return "[NativeFunction " + nativeFn.functionName() + "]";
}

NativeFunctionObject& NativeFunctionType::requireNativeFunction(Object& self) {
    auto* nativeFn = dynamic_cast<NativeFunctionObject*>(&self);
    if (!nativeFn) {
        throw std::runtime_error("NativeFunctionType called with non-native-function object");
    }
    return *nativeFn;
}

} // namespace gs
