#pragma once

#include "gs/type_system/type_base.hpp"

namespace gs {

class ListObject : public Object {
public:
    explicit ListObject(const Type& typeRef);
    ListObject(const Type& typeRef, std::vector<Value> values);

    const Type& getType() const override;
    std::vector<Value>& data();
    const std::vector<Value>& data() const;

private:
    const Type* type_;
    std::vector<Value> data_;
};

class ListType : public Type {
public:
    using MethodHandler = Value (ListType::*)(Object& self, const std::vector<Value>& args) const;

    ListType();
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
    static ListObject& requireList(Object& self);

    Value methodPush(Object& self, const std::vector<Value>& args) const;
    Value methodGet(Object& self, const std::vector<Value>& args) const;
    Value methodSet(Object& self, const std::vector<Value>& args) const;
    Value methodRemove(Object& self, const std::vector<Value>& args) const;
    Value methodSize(Object& self, const std::vector<Value>& args) const;
    Value methodSort(Object& self, const std::vector<Value>& args) const;
    Value memberLengthGet(Object& self) const;
    Value memberLengthSet(Object& self, const Value& value) const;
};

} // namespace gs
