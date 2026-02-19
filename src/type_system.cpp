#include "gs/type_system.hpp"

#include <sstream>
#include <stdexcept>

namespace gs {

Type::Type() {
    registerMethodAttribute("__str__", 0, [this](Object& self,
                                                 const std::vector<Value>& args,
                                                 const StringFactory& makeString,
                                                 const ValueStrInvoker& valueStr) {
        (void)args;
        return makeString(__str__(self, valueStr));
    });
}

void Type::registerMethodAttribute(const std::string& name, std::size_t argc, MethodInvoker invoker) {
    AttributeEntry entry;
    entry.argc = argc;
    entry.method = std::move(invoker);
    attributes_[name] = std::move(entry);
}

void Type::registerMethodAttribute(const std::string& name, std::size_t argc, SimpleMethodInvoker invoker) {
    registerMethodAttribute(
        name,
        argc,
        [simple = std::move(invoker)](Object& self,
                                      const std::vector<Value>& args,
                                      const StringFactory& makeString,
                                      const ValueStrInvoker& valueStr) {
            (void)makeString;
            (void)valueStr;
            return simple(self, args);
        });
}

void Type::registerMemberAttribute(const std::string& name, GetterInvoker getter, SetterInvoker setter) {
    AttributeEntry entry;
    entry.getter = std::move(getter);
    entry.setter = std::move(setter);
    attributes_[name] = std::move(entry);
}

Value Type::callMethod(Object& self,
                       const std::string& method,
                       const std::vector<Value>& args,
                       const StringFactory& makeString,
                       const ValueStrInvoker& valueStr) const {
    auto it = attributes_.find(method);
    if (it != attributes_.end() && it->second.method) {
        if (args.size() != it->second.argc) {
            throw std::runtime_error(std::string(name()) + "." + method + " argument count mismatch");
        }
        return it->second.method(self, args, makeString, valueStr);
    }

    throw std::runtime_error("Unknown " + std::string(name()) + " method: " + method);
}

Value Type::getMember(Object& self, const std::string& member) const {
    auto it = attributes_.find(member);
    if (it != attributes_.end() && it->second.getter) {
        return it->second.getter(self);
    }

    (void)self;
    throw std::runtime_error("Unknown " + std::string(name()) + " member: " + member);
}

Value Type::setMember(Object& self, const std::string& member, const Value& value) const {
    auto it = attributes_.find(member);
    if (it != attributes_.end() && it->second.setter) {
        return it->second.setter(self, value);
    }

    (void)self;
    (void)value;
    throw std::runtime_error("Unknown or read-only " + std::string(name()) + " member: " + member);
}

std::string Type::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    (void)valueStr;
    return std::string(name()) + "#" + std::to_string(self.objectId());
}

ListObject::ListObject(const Type& typeRef) : type_(&typeRef) {}
ListObject::ListObject(const Type& typeRef, std::vector<Value> values)
    : type_(&typeRef), data_(std::move(values)) {}

const Type& ListObject::getType() const {
    return *type_;
}

std::vector<Value>& ListObject::data() {
    return data_;
}

const std::vector<Value>& ListObject::data() const {
    return data_;
}

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

const char* ListType::name() const {
    return "List";
}

ListType::ListType() {
    registerMethodAttribute("__str__", 0, [this](Object& self,
                                                  const std::vector<Value>& args,
                                                  const StringFactory& makeString,
                                                  const ValueStrInvoker& valueStr) {
        (void)args;
        return makeString(__str__(self, valueStr));
    });
    registerMethod("push", 1, &ListType::methodPush);
    registerMethod("get", 1, &ListType::methodGet);
    registerMethod("set", 2, &ListType::methodSet);
    registerMethod("remove", 1, &ListType::methodRemove);
    registerMethod("size", 0, &ListType::methodSize);

    registerMemberAttribute(
        "length",
        [this](Object& self) { return memberLengthGet(self); },
        [this](Object& self, const Value& value) { return memberLengthSet(self, value); });
}

void ListType::registerMethod(const std::string& name, std::size_t argc, MethodHandler handler) {
    registerMethodAttribute(name, argc, [this, handler](Object& self, const std::vector<Value>& args) {
        return (this->*handler)(self, args);
    });
}

Value ListType::callMethod(Object& self,
                           const std::string& method,
                           const std::vector<Value>& args,
                           const StringFactory& makeString,
                           const ValueStrInvoker& valueStr) const {
    return Type::callMethod(self, method, args, makeString, valueStr);
}

Value ListType::getMember(Object& self, const std::string& member) const {
    return Type::getMember(self, member);
}

Value ListType::setMember(Object& self, const std::string& member, const Value& value) const {
    return Type::setMember(self, member, value);
}

std::string ListType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto& list = requireList(self);
    std::ostringstream ss;
    ss << "[";
    for (std::size_t i = 0; i < list.data().size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        ss << valueStr(list.data()[i]);
    }
    ss << "]";
    return ss.str();
}

ListObject& ListType::requireList(Object& self) {
    auto* list = dynamic_cast<ListObject*>(&self);
    if (!list) {
        throw std::runtime_error("ListType called with non-list object");
    }
    return *list;
}

Value ListType::methodPush(Object& self, const std::vector<Value>& args) const {
    auto& list = requireList(self);
    list.data().push_back(args[0]);
    return Value::Int(static_cast<std::int64_t>(list.data().size()));
}

Value ListType::methodGet(Object& self, const std::vector<Value>& args) const {
    auto& list = requireList(self);
    const auto index = static_cast<std::size_t>(args[0].asInt());
    if (index >= list.data().size()) {
        return Value::Nil();
    }
    return list.data()[index];
}

Value ListType::methodSet(Object& self, const std::vector<Value>& args) const {
    auto& list = requireList(self);
    const auto index = static_cast<std::size_t>(args[0].asInt());
    if (index >= list.data().size()) {
        throw std::runtime_error("List.set index out of range");
    }
    list.data()[index] = args[1];
    return args[1];
}

Value ListType::methodRemove(Object& self, const std::vector<Value>& args) const {
    auto& list = requireList(self);
    const auto index = static_cast<std::size_t>(args[0].asInt());
    if (index >= list.data().size()) {
        return Value::Nil();
    }
    Value removed = list.data()[index];
    list.data().erase(list.data().begin() + static_cast<std::ptrdiff_t>(index));
    return removed;
}

Value ListType::methodSize(Object& self, const std::vector<Value>& args) const {
    auto& list = requireList(self);
    (void)args;
    return Value::Int(static_cast<std::int64_t>(list.data().size()));
}

Value ListType::memberLengthGet(Object& self) const {
    auto& list = requireList(self);
    return Value::Int(static_cast<std::int64_t>(list.data().size()));
}

Value ListType::memberLengthSet(Object& self, const Value& value) const {
    (void)self;
    (void)value;
    throw std::runtime_error("List.length is read-only");
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

FunctionObject::FunctionObject(const Type& typeRef, std::size_t functionIndex)
    : type_(&typeRef), functionIndex_(functionIndex) {}

const Type& FunctionObject::getType() const {
    return *type_;
}

std::size_t FunctionObject::functionIndex() const {
    return functionIndex_;
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

ScriptInstanceObject::ScriptInstanceObject(const Type& typeRef, std::size_t classIndex, std::string className)
    : type_(&typeRef), classIndex_(classIndex), className_(std::move(className)) {}

const Type& ScriptInstanceObject::getType() const {
    return *type_;
}

std::size_t ScriptInstanceObject::classIndex() const {
    return classIndex_;
}

const std::string& ScriptInstanceObject::className() const {
    return className_;
}

std::unordered_map<std::string, Value>& ScriptInstanceObject::fields() {
    return fields_;
}

const std::unordered_map<std::string, Value>& ScriptInstanceObject::fields() const {
    return fields_;
}

const char* ScriptInstanceType::name() const {
    return "Instance";
}

ScriptInstanceType::ScriptInstanceType() {
    registerMethodAttribute("__str__", 0, [this](Object& self,
                                                  const std::vector<Value>& args,
                                                  const StringFactory& makeString,
                                                  const ValueStrInvoker& valueStr) {
        (void)args;
        return makeString(__str__(self, valueStr));
    });
}

Value ScriptInstanceType::callMethod(Object& self,
                                     const std::string& method,
                                     const std::vector<Value>& args,
                                     const StringFactory& makeString,
                                     const ValueStrInvoker& valueStr) const {
    auto* instance = dynamic_cast<ScriptInstanceObject*>(&self);
    if (!instance) {
        throw std::runtime_error("ScriptInstanceType called with non-instance object");
    }

    (void)instance;
    return Type::callMethod(self, method, args, makeString, valueStr);
}

std::string ScriptInstanceType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto* instance = dynamic_cast<ScriptInstanceObject*>(&self);
    if (!instance) {
        throw std::runtime_error("ScriptInstanceType called with non-instance object");
    }

    (void)valueStr;
    return instance->className() + "#" + std::to_string(instance->objectId());
}

} // namespace gs
