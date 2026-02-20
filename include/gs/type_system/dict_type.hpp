#pragma once

#include "gs/type_system/type_base.hpp"

namespace gs {

class DictObject : public Object {
public:
    explicit DictObject(const Type& typeRef);
    DictObject(const Type& typeRef, std::unordered_map<std::int64_t, Value> values);

    const Type& getType() const override;
    std::unordered_map<std::int64_t, Value>& data();
    const std::unordered_map<std::int64_t, Value>& data() const;

private:
    const Type* type_;
    std::unordered_map<std::int64_t, Value> data_;
};

class DictType : public Type {
public:
    using MethodHandler = Value (DictType::*)(Object& self, const std::vector<Value>& args) const;

    DictType();
    const char* name() const override;
    Value callMethod(Object& self,
                     const std::string& method,
                     const std::vector<Value>& args,
                     const StringFactory& makeString,
                     const ValueStrInvoker& valueStr) const override;
    Value getMember(Object& self, const std::string& member) const override;
    Value setMember(Object& self, const std::string& member, const Value& value) const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;

    void registerMethod(const std::string& name, std::size_t argc, MethodHandler handler);

private:
    static DictObject& requireDict(Object& self);

    Value methodSet(Object& self, const std::vector<Value>& args) const;
    Value methodGet(Object& self, const std::vector<Value>& args) const;
    Value methodDel(Object& self, const std::vector<Value>& args) const;
    Value methodSize(Object& self, const std::vector<Value>& args) const;
    Value methodKeyAt(Object& self, const std::vector<Value>& args) const;
    Value methodValueAt(Object& self, const std::vector<Value>& args) const;
    Value memberLengthGet(Object& self) const;
    Value memberLengthSet(Object& self, const Value& value) const;
};

} // namespace gs
