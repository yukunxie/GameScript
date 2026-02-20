#include "gs/type_system/dict_type.hpp"

#include <sstream>
#include <stdexcept>

namespace gs {

DictObject::DictObject(const Type& typeRef) : type_(&typeRef) {}
DictObject::DictObject(const Type& typeRef, std::unordered_map<std::int64_t, Value> values)
    : type_(&typeRef), data_(std::move(values)) {}

const Type& DictObject::getType() const {
    return *type_;
}

std::unordered_map<std::int64_t, Value>& DictObject::data() {
    return data_;
}

const std::unordered_map<std::int64_t, Value>& DictObject::data() const {
    return data_;
}

const char* DictType::name() const {
    return "Dict";
}

DictType::DictType() {
    registerMethodAttribute("__str__", 0, [this](Object& self,
                                                  const std::vector<Value>& args,
                                                  const StringFactory& makeString,
                                                  const ValueStrInvoker& valueStr) {
        (void)args;
        return makeString(__str__(self, valueStr));
    });
    registerMethod("set", 2, &DictType::methodSet);
    registerMethod("get", 1, &DictType::methodGet);
    registerMethod("del", 1, &DictType::methodDel);
    registerMethod("size", 0, &DictType::methodSize);
    registerMethod("key_at", 1, &DictType::methodKeyAt);
    registerMethod("value_at", 1, &DictType::methodValueAt);

    registerMemberAttribute(
        "length",
        [this](Object& self) { return memberLengthGet(self); },
        [this](Object& self, const Value& value) { return memberLengthSet(self, value); });
}

void DictType::registerMethod(const std::string& name, std::size_t argc, MethodHandler handler) {
    registerMethodAttribute(name, argc, [this, handler](Object& self, const std::vector<Value>& args) {
        return (this->*handler)(self, args);
    });
}

Value DictType::callMethod(Object& self,
                           const std::string& method,
                           const std::vector<Value>& args,
                           const StringFactory& makeString,
                           const ValueStrInvoker& valueStr) const {
    return Type::callMethod(self, method, args, makeString, valueStr);
}

Value DictType::getMember(Object& self, const std::string& member) const {
    return Type::getMember(self, member);
}

Value DictType::setMember(Object& self, const std::string& member, const Value& value) const {
    return Type::setMember(self, member, value);
}

std::string DictType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto& dict = requireDict(self);
    std::ostringstream ss;
    ss << "{";
    bool first = true;
    for (const auto& kv : dict.data()) {
        if (!first) {
            ss << ", ";
        }
        first = false;
        ss << kv.first << ": " << valueStr(kv.second);
    }
    ss << "}";
    return ss.str();
}

DictObject& DictType::requireDict(Object& self) {
    auto* dict = dynamic_cast<DictObject*>(&self);
    if (!dict) {
        throw std::runtime_error("DictType called with non-dict object");
    }
    return *dict;
}

Value DictType::methodSet(Object& self, const std::vector<Value>& args) const {
    auto& dict = requireDict(self);
    const auto key = args[0].asInt();
    dict.data()[key] = args[1];
    return args[1];
}

Value DictType::methodGet(Object& self, const std::vector<Value>& args) const {
    auto& dict = requireDict(self);
    const auto key = args[0].asInt();
    auto it = dict.data().find(key);
    if (it == dict.data().end()) {
        return Value::Nil();
    }
    return it->second;
}

Value DictType::methodDel(Object& self, const std::vector<Value>& args) const {
    auto& dict = requireDict(self);
    const auto key = args[0].asInt();
    auto it = dict.data().find(key);
    if (it == dict.data().end()) {
        return Value::Nil();
    }
    Value removed = it->second;
    dict.data().erase(it);
    return removed;
}

Value DictType::methodSize(Object& self, const std::vector<Value>& args) const {
    auto& dict = requireDict(self);
    (void)args;
    return Value::Int(static_cast<std::int64_t>(dict.data().size()));
}

Value DictType::methodKeyAt(Object& self, const std::vector<Value>& args) const {
    auto& dict = requireDict(self);
    const auto index = static_cast<std::size_t>(args[0].asInt());
    if (index >= dict.data().size()) {
        return Value::Nil();
    }
    auto it = dict.data().begin();
    std::advance(it, static_cast<std::ptrdiff_t>(index));
    return Value::Int(it->first);
}

Value DictType::methodValueAt(Object& self, const std::vector<Value>& args) const {
    auto& dict = requireDict(self);
    const auto index = static_cast<std::size_t>(args[0].asInt());
    if (index >= dict.data().size()) {
        return Value::Nil();
    }
    auto it = dict.data().begin();
    std::advance(it, static_cast<std::ptrdiff_t>(index));
    return it->second;
}

Value DictType::memberLengthGet(Object& self) const {
    auto& dict = requireDict(self);
    return Value::Int(static_cast<std::int64_t>(dict.data().size()));
}

Value DictType::memberLengthSet(Object& self, const Value& value) const {
    (void)self;
    (void)value;
    throw std::runtime_error("Dict.length is read-only");
}

} // namespace gs
