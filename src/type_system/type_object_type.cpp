#include "gs/type_system/type_object_type.hpp"
#include "gs/binding.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace gs {

// ============================================================================
// TypeObject Implementation
// ============================================================================

TypeObject::TypeObject(const Type& typeRef, const std::string& typeName)
    : typeRef_(typeRef), typeName_(typeName) {}

const Type& TypeObject::getType() const {
    static TypeObjectType typeObjectType;
    return typeObjectType;
}

Value TypeObject::convert(HostContext& context, const Value& value) const {
    // Handle conversion based on type name
    if (typeName_ == "Int") {
        if (value.isInt()) {
            return value;
        }
        if (value.isFloat()) {
            return Value::Int(static_cast<std::int64_t>(value.asFloat()));
        }
        if (value.isBool()) {
            return Value::Int(value.asBool() ? 1 : 0);
        }
        if (value.isString()) {
            try {
                const std::string& str = context.__str__(value);
                return Value::Int(std::stoll(str));
            } catch (...) {
                throw std::runtime_error("Cannot convert string to Int: " + context.__str__(value));
            }
        }
        if (value.isNil()) {
            return Value::Int(0);
        }
        throw std::runtime_error("Cannot convert " + context.typeName(value) + " to Int");
    }
    
    if (typeName_ == "Float") {
        if (value.isFloat()) {
            return value;
        }
        if (value.isInt()) {
            return Value::Float(static_cast<double>(value.asInt()));
        }
        if (value.isBool()) {
            return Value::Float(value.asBool() ? 1.0 : 0.0);
        }
        if (value.isString()) {
            try {
                const std::string& str = context.__str__(value);
                return Value::Float(std::stod(str));
            } catch (...) {
                throw std::runtime_error("Cannot convert string to Float: " + context.__str__(value));
            }
        }
        if (value.isNil()) {
            return Value::Float(0.0);
        }
        throw std::runtime_error("Cannot convert " + context.typeName(value) + " to Float");
    }
    
    if (typeName_ == "Bool") {
        if (value.isBool()) {
            return value;
        }
        if (value.isInt()) {
            return Value::Bool(value.asInt() != 0);
        }
        if (value.isFloat()) {
            return Value::Bool(std::abs(value.asFloat()) > std::numeric_limits<double>::epsilon());
        }
        if (value.isNil()) {
            return Value::Bool(false);
        }
        // All other types (strings, refs) are truthy
        return Value::Bool(true);
    }
    
    if (typeName_ == "String") {
        return context.createString(context.__str__(value));
    }
    
    // For other types, just return the value as-is
    return value;
}

// ============================================================================
// TypeObjectType Implementation
// ============================================================================

TypeObjectType::TypeObjectType() {
    // TypeObject can be called like a function to convert values
    // This will be handled in the VM when calling a TypeObject
}

const char* TypeObjectType::name() const {
    return "Type";
}

std::string TypeObjectType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto* typeObj = dynamic_cast<TypeObject*>(&self);
    if (!typeObj) {
        return "<Type>";
    }
    return "<type '" + typeObj->typeName() + "'>";
}

} // namespace gs
