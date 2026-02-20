#include "gs/type_system/type_base.hpp"

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

} // namespace gs
