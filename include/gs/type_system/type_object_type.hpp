#pragma once

#include "gs/type_system/type_base.hpp"
#include <memory>
#include <string>

namespace gs {

// Forward declaration
class HostContext;

// TypeObject represents a runtime type (like IntType, StringType, etc.)
// It can be called as a constructor to create instances of that type
class TypeObject final : public Object {
public:
    explicit TypeObject(const Type& typeRef, const std::string& typeName);
    
    const Type& getType() const override;
    const std::string& typeName() const { return typeName_; }
    
    // Convert a value to this type (used when TypeObject is called)
    Value convert(HostContext& context, const Value& value) const;

private:
    const Type& typeRef_;
    std::string typeName_;
};

// TypeObjectType is the type of all TypeObject instances
class TypeObjectType final : public Type {
public:
    TypeObjectType();
    const char* name() const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
};

} // namespace gs
