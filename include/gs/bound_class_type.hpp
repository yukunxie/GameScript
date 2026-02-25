#pragma once

#include "gs/binding.hpp"
#include "gs/type_system/type_base.hpp"

#include <functional>
#include <string>
#include <unordered_map>

namespace gs {

// ============================================================================
// BoundClassType - Custom Type class that supports V2 Binding API
// ============================================================================
// This Type subclass stores member accessors and method invokers that require
// HostContext, enabling the V2 binding API to work with native C++ classes.
// 
// Usage:
//   static BoundClassType myType("MyClass");
//   gs::registerNativeType(typeid(MyClass), myType);
//   
//   auto binder = bindings.beginClass<MyClass>("MyClass");
//   binder.constructor<MyClass>();
//   binder.field<MyClass>("x", &MyClass::x);
//   binder.finalize();
// ============================================================================

class BoundClassType : public Type {
public:
    explicit BoundClassType(std::string name);
    
    const char* name() const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
    
    // Override getMember to support HostContext-aware getters
    Value getMember(Object& self, const std::string& member) const override;
    
    // Override setMember to support HostContext-aware setters
    Value setMember(Object& self, const std::string& member, const Value& value) const override;
    
    // Override callMethod to support HostContext-aware methods
    Value callMethod(Object& self,
                    const std::string& method,
                    const std::vector<Value>& args,
                    const StringFactory& makeString,
                    const ValueStrInvoker& valueStr) const override;
    
    // Register member accessors (used by ClassBinder)
    void registerGetter(const std::string& name,
                       std::function<Value(HostContext&, Object&)> getter);
    
    void registerSetter(const std::string& name,
                       std::function<Value(HostContext&, Object&, const Value&)> setter);
    
    // Register method invokers (used by ClassBinder)
    void registerMethod(const std::string& name,
                       std::function<Value(HostContext&, Object&, const std::vector<Value>&)> method);
    
    // Set the HostContext for the current thread (called by VM before member access)
    static void setThreadLocalContext(HostContext* ctx);
    static HostContext* getThreadLocalContext();
    
private:
    std::string name_;
    std::unordered_map<std::string, 
        std::function<Value(HostContext&, Object&)>> memberGetters_;
    std::unordered_map<std::string,
        std::function<Value(HostContext&, Object&, const Value&)>> memberSetters_;
    std::unordered_map<std::string,
        std::function<Value(HostContext&, Object&, const std::vector<Value>&)>> methods_;
    
    // Thread-local storage for HostContext
    static thread_local HostContext* threadContext_;
};

} // namespace gs
