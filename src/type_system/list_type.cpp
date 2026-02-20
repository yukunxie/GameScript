#include "gs/type_system/list_type.hpp"

#include <sstream>
#include <stdexcept>

namespace gs {

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

} // namespace gs
