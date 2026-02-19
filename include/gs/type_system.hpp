#pragma once

#include "gs/bytecode.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gs {

class Object;

class Type {
public:
    using StringFactory = std::function<Value(const std::string& text)>;
    using ValueStrInvoker = std::function<std::string(const Value& value)>;
    using MethodInvoker = std::function<Value(Object& self,
                                              const std::vector<Value>& args,
                                              const StringFactory& makeString,
                                              const ValueStrInvoker& valueStr)>;
    using SimpleMethodInvoker = std::function<Value(Object& self, const std::vector<Value>& args)>;
    using GetterInvoker = std::function<Value(Object& self)>;
    using SetterInvoker = std::function<Value(Object& self, const Value& value)>;

    struct AttributeEntry {
        std::size_t argc{0};
        MethodInvoker method;
        GetterInvoker getter;
        SetterInvoker setter;
    };

    Type();
    virtual ~Type() = default;
    virtual const char* name() const = 0;
    virtual Value callMethod(Object& self,
                             const std::string& method,
                             const std::vector<Value>& args,
                             const StringFactory& makeString,
                             const ValueStrInvoker& valueStr) const;
    virtual Value getMember(Object& self, const std::string& member) const;
    virtual Value setMember(Object& self, const std::string& member, const Value& value) const;
    virtual std::string __str__(Object& self, const ValueStrInvoker& valueStr) const;

protected:
    void registerMethodAttribute(const std::string& name, std::size_t argc, MethodInvoker invoker);
    void registerMethodAttribute(const std::string& name, std::size_t argc, SimpleMethodInvoker invoker);
    void registerMemberAttribute(const std::string& name, GetterInvoker getter, SetterInvoker setter);

    std::unordered_map<std::string, AttributeEntry> attributes_;
};

class Object {
public:
    virtual ~Object() = default;
    virtual const Type& getType() const = 0;
    void setObjectId(std::uint64_t id) { objectId_ = id; }
    std::uint64_t objectId() const { return objectId_; }

private:
    std::uint64_t objectId_{0};
};

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
    Value memberLengthGet(Object& self) const;
    Value memberLengthSet(Object& self, const Value& value) const;
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

class FunctionObject : public Object {
public:
    FunctionObject(const Type& typeRef, std::size_t functionIndex);

    const Type& getType() const override;
    std::size_t functionIndex() const;

private:
    const Type* type_;
    std::size_t functionIndex_{0};
};

class FunctionType : public Type {
public:
    FunctionType();
    const char* name() const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
};

class ScriptInstanceObject : public Object {
public:
    ScriptInstanceObject(const Type& typeRef, std::size_t classIndex, std::string className);

    const Type& getType() const override;
    std::size_t classIndex() const;
    const std::string& className() const;
    std::unordered_map<std::string, Value>& fields();
    const std::unordered_map<std::string, Value>& fields() const;

private:
    const Type* type_;
    std::size_t classIndex_{0};
    std::string className_;
    std::unordered_map<std::string, Value> fields_;
};

class ScriptInstanceType : public Type {
public:
    ScriptInstanceType();
    const char* name() const override;
    Value callMethod(Object& self,
                     const std::string& method,
                     const std::vector<Value>& args,
                     const StringFactory& makeString,
                     const ValueStrInvoker& valueStr) const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
};

} // namespace gs
