#include "gs/type_system/dict_type.hpp"
#include "gs/type_system/list_type.hpp"
#include "gs/type_system/string_type.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace gs {

namespace {

std::string escapeJson(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (const char ch : text) {
        switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            out += ch;
            break;
        }
    }
    return out;
}

std::string toJsonValue(const Value& value,
                        const Type::ValueStrInvoker& valueStr,
                        std::unordered_set<const Object*>& visiting);

std::string toJsonObject(const DictObject& dict,
                         const Type::ValueStrInvoker& valueStr,
                         std::unordered_set<const Object*>& visiting) {
    std::ostringstream ss;
    ss << "{";
    bool first = true;
    for (const auto& kv : dict.data()) {
        if (!first) {
            ss << ",";
        }
        first = false;
        ss << "\"" << escapeJson(valueStr(kv.first)) << "\":"
           << toJsonValue(kv.second, valueStr, visiting);
    }
    ss << "}";
    return ss.str();
}

std::string toJsonArray(const ListObject& list,
                        const Type::ValueStrInvoker& valueStr,
                        std::unordered_set<const Object*>& visiting) {
    std::ostringstream ss;
    ss << "[";
    for (std::size_t i = 0; i < list.data().size(); ++i) {
        if (i > 0) {
            ss << ",";
        }
        ss << toJsonValue(list.data()[i], valueStr, visiting);
    }
    ss << "]";
    return ss.str();
}

std::string toJsonValue(const Value& value,
                        const Type::ValueStrInvoker& valueStr,
                        std::unordered_set<const Object*>& visiting) {
    if (value.isNil()) {
        return "null";
    }
    if (value.isBool()) {
        return value.asBool() ? "true" : "false";
    }
    if (value.isInt()) {
        return std::to_string(value.asInt());
    }
    if (value.isFloat()) {
        std::ostringstream out;
        out << value.asFloat();
        return out.str();
    }
    if (value.isString() || value.isLegacyStringLiteral()) {
        return "\"" + escapeJson(valueStr(value)) + "\"";
    }
    if (value.isRef()) {
        Object* object = value.asRef();
        if (!object) {
            return "null";
        }
        if (auto* stringObject = dynamic_cast<StringObject*>(object)) {
            return "\"" + escapeJson(stringObject->data()) + "\"";
        }
        if (visiting.contains(object)) {
            return "\"[Circular]\"";
        }

        if (auto* dictObject = dynamic_cast<DictObject*>(object)) {
            visiting.insert(object);
            const std::string out = toJsonObject(*dictObject, valueStr, visiting);
            visiting.erase(object);
            return out;
        }
        if (auto* listObject = dynamic_cast<ListObject*>(object)) {
            visiting.insert(object);
            const std::string out = toJsonArray(*listObject, valueStr, visiting);
            visiting.erase(object);
            return out;
        }

        return "\"" + escapeJson(valueStr(value)) + "\"";
    }

    return "\"" + escapeJson(valueStr(value)) + "\"";
}

} // namespace

DictObject::DictObject(const Type& typeRef) : type_(&typeRef) {}

DictObject::DictObject(const Type& typeRef, MapType values)
    : type_(&typeRef), data_(std::move(values)) {}

// Legacy constructor for backward compatibility with int keys
DictObject::DictObject(const Type& typeRef, std::unordered_map<std::int64_t, Value> intValues)
    : type_(&typeRef) {
    for (auto& kv : intValues) {
        data_[Value::Int(kv.first)] = std::move(kv.second);
    }
}

const Type& DictObject::getType() const {
    return *type_;
}

DictObject::MapType& DictObject::data() {
    return data_;
}

const DictObject::MapType& DictObject::data() const {
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
    std::unordered_set<const Object*> visiting;
    visiting.insert(&dict);
    return toJsonObject(dict, valueStr, visiting);
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
    const Value& key = args[0];
    dict.data()[key] = args[1];
    return args[1];
}

Value DictType::methodGet(Object& self, const std::vector<Value>& args) const {
    auto& dict = requireDict(self);
    const Value& key = args[0];
    auto it = dict.data().find(key);
    if (it == dict.data().end()) {
        throw std::runtime_error("Dict key not found");
    }
    return it->second;
}

Value DictType::methodDel(Object& self, const std::vector<Value>& args) const {
    auto& dict = requireDict(self);
    const Value& key = args[0];
    auto it = dict.data().find(key);
    if (it == dict.data().end()) {
        throw std::runtime_error("Dict key not found");
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
    return it->first;
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