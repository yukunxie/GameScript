#include "gs/bound_class_type.hpp"

#include <stdexcept>

namespace gs {

// Initialize thread-local storage
thread_local HostContext* BoundClassType::threadContext_ = nullptr;

BoundClassType::BoundClassType(std::string name) 
    : name_(std::move(name)) {}

const char* BoundClassType::name() const {
    return name_.c_str();
}

std::string BoundClassType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    return "<" + name_ + " object>";
}

Value BoundClassType::getMember(Object& self, const std::string& member) const {
    auto it = memberGetters_.find(member);
    if (it != memberGetters_.end()) {
        HostContext* ctx = getThreadLocalContext();
        if (!ctx) {
            throw std::runtime_error("HostContext not available for member access: " + member);
        }
        return it->second(*ctx, self);
    }
    
    // Fall back to base class (will throw "Unknown member" if not found)
    return Type::getMember(self, member);
}

Value BoundClassType::setMember(Object& self, const std::string& member, const Value& value) const {
    auto it = memberSetters_.find(member);
    if (it != memberSetters_.end()) {
        HostContext* ctx = getThreadLocalContext();
        if (!ctx) {
            throw std::runtime_error("HostContext not available for member assignment: " + member);
        }
        return it->second(*ctx, self, value);
    }
    
    // Fall back to base class (will throw "Unknown or read-only member" if not found)
    return Type::setMember(self, member, value);
}

Value BoundClassType::callMethod(Object& self,
                                 const std::string& method,
                                 const std::vector<Value>& args,
                                 const StringFactory& makeString,
                                 const ValueStrInvoker& valueStr) const {
    auto it = methods_.find(method);
    if (it != methods_.end()) {
        HostContext* ctx = getThreadLocalContext();
        if (!ctx) {
            throw std::runtime_error("HostContext not available for method call: " + method);
        }
        return it->second(*ctx, self, args);
    }
    
    // Fall back to base class (will throw "Unknown method" if not found)
    return Type::callMethod(self, method, args, makeString, valueStr);
}

void BoundClassType::registerGetter(const std::string& name,
                                   std::function<Value(HostContext&, Object&)> getter) {
    memberGetters_[name] = std::move(getter);
}

void BoundClassType::registerSetter(const std::string& name,
                                   std::function<Value(HostContext&, Object&, const Value&)> setter) {
    memberSetters_[name] = std::move(setter);
}

void BoundClassType::registerMethod(const std::string& name,
                                   std::function<Value(HostContext&, Object&, const std::vector<Value>&)> method) {
    methods_[name] = std::move(method);
}

void BoundClassType::setThreadLocalContext(HostContext* ctx) {
    threadContext_ = ctx;
}

HostContext* BoundClassType::getThreadLocalContext() {
    return threadContext_;
}

} // namespace gs
