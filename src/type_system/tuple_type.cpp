#include "gs/type_system/tuple_type.hpp"

#include <sstream>
#include <stdexcept>

namespace gs {

TupleObject::TupleObject(const Type& typeRef, std::vector<Value> values)
    : type_(&typeRef), data_(std::move(values)) {}

const Type& TupleObject::getType() const {
    return *type_;
}

std::vector<Value>& TupleObject::data() {
    return data_;
}

const std::vector<Value>& TupleObject::data() const {
    return data_;
}

const char* TupleType::name() const {
    return "Tuple";
}

TupleType::TupleType() {
    registerMethodAttribute("__str__", 0, [this](Object& self,
                                                  const std::vector<Value>& args,
                                                  const StringFactory& makeString,
                                                  const ValueStrInvoker& valueStr) {
        (void)args;
        return makeString(__str__(self, valueStr));
    });
    registerMethod("get", 1, &TupleType::methodGet);
    registerMethod("set", 2, &TupleType::methodSet);
    registerMethod("size", 0, &TupleType::methodSize);

    registerMemberAttribute(
        "length",
        [this](Object& self) { return memberLengthGet(self); },
        [this](Object& self, const Value& value) { return memberLengthSet(self, value); });
}

void TupleType::registerMethod(const std::string& name, std::size_t argc, MethodHandler handler) {
    registerMethodAttribute(name, argc, [this, handler](Object& self, const std::vector<Value>& args) {
        return (this->*handler)(self, args);
    });
}

Value TupleType::callMethod(Object& self,
                            const std::string& method,
                            const std::vector<Value>& args,
                            const StringFactory& makeString,
                            const ValueStrInvoker& valueStr) const {
    return Type::callMethod(self, method, args, makeString, valueStr);
}

Value TupleType::getMember(Object& self, const std::string& member) const {
    return Type::getMember(self, member);
}

Value TupleType::setMember(Object& self, const std::string& member, const Value& value) const {
    return Type::setMember(self, member, value);
}

std::string TupleType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto& tuple = requireTuple(self);
    std::ostringstream ss;
    ss << "(";
    for (std::size_t i = 0; i < tuple.data().size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        ss << valueStr(tuple.data()[i]);
    }
    ss << ")";
    return ss.str();
}

TupleObject& TupleType::requireTuple(Object& self) {
    auto* tuple = dynamic_cast<TupleObject*>(&self);
    if (!tuple) {
        throw std::runtime_error("TupleType called with non-tuple object");
    }
    return *tuple;
}

Value TupleType::methodGet(Object& self, const std::vector<Value>& args) const {
    auto& tuple = requireTuple(self);
    const auto index = static_cast<std::size_t>(args[0].asInt());
    if (index >= tuple.data().size()) {
        return Value::Nil();
    }
    return tuple.data()[index];
}

Value TupleType::methodSet(Object& self, const std::vector<Value>& args) const {
    auto& tuple = requireTuple(self);
    const auto index = static_cast<std::size_t>(args[0].asInt());
    if (index >= tuple.data().size()) {
        throw std::runtime_error("Tuple.set index out of range");
    }
    tuple.data()[index] = args[1];
    return args[1];
}

Value TupleType::methodSize(Object& self, const std::vector<Value>& args) const {
    auto& tuple = requireTuple(self);
    (void)args;
    return Value::Int(static_cast<std::int64_t>(tuple.data().size()));
}

Value TupleType::memberLengthGet(Object& self) const {
    auto& tuple = requireTuple(self);
    return Value::Int(static_cast<std::int64_t>(tuple.data().size()));
}

Value TupleType::memberLengthSet(Object& self, const Value& value) const {
    (void)self;
    (void)value;
    throw std::runtime_error("Tuple.length is read-only");
}

} // namespace gs
