#pragma once

#include "gs/bytecode.hpp"

#include <cstdint>
#include <functional>
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
    
    // Make registration methods public for V2 binding API
    void registerMethodAttribute(const std::string& name, std::size_t argc, MethodInvoker invoker);
    void registerMethodAttribute(const std::string& name, std::size_t argc, SimpleMethodInvoker invoker);
    void registerMemberAttribute(const std::string& name, GetterInvoker getter, SetterInvoker setter);

protected:
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

} // namespace gs
