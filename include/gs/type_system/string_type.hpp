#pragma once

#include "gs/type_system/type_base.hpp"
#include <string>
#include <vector>

namespace gs {

class StringObject : public Object {
public:
    explicit StringObject(const Type& typeRef, const std::string& text);

    const Type& getType() const override;
    std::string& data();
    const std::string& data() const;
    std::size_t size() const;
    char at(std::size_t index) const;
    
    // String-specific operations
    bool contains(const std::string& substr) const;
    std::size_t find(const std::string& substr, std::size_t pos = 0) const;
    std::string substr(std::size_t pos, std::size_t len = std::string::npos) const;
    std::vector<std::string> split(const std::string& delimiter) const;
    std::string replace(const std::string& from, const std::string& to) const;

private:
    const Type* type_;
    std::string data_;
};

class StringType : public Type {
public:
    using MethodHandler = Value (StringType::*)(Object& self,
                                                const std::vector<Value>& args,
                                                const StringFactory& makeString,
                                                const ValueStrInvoker& valueStr) const;

    StringType();
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

    // Static helper for accessing string content from Values
    static std::string getStringContent(const Value& value, const ValueStrInvoker& valueStr);

private:
    static StringObject& requireString(Object& self);

    // String methods
    Value methodSize(Object& self,
                     const std::vector<Value>& args,
                     const StringFactory& makeString,
                     const ValueStrInvoker& valueStr) const;
    Value methodLength(Object& self,
                       const std::vector<Value>& args,
                       const StringFactory& makeString,
                       const ValueStrInvoker& valueStr) const;
    Value methodContains(Object& self,
                         const std::vector<Value>& args,
                         const StringFactory& makeString,
                         const ValueStrInvoker& valueStr) const;
    Value methodFind(Object& self,
                     const std::vector<Value>& args,
                     const StringFactory& makeString,
                     const ValueStrInvoker& valueStr) const;
    Value methodSubstr(Object& self,
                       const std::vector<Value>& args,
                       const StringFactory& makeString,
                       const ValueStrInvoker& valueStr) const;
    Value methodSlice(Object& self,
                      const std::vector<Value>& args,
                      const StringFactory& makeString,
                      const ValueStrInvoker& valueStr) const;
    Value methodSplit(Object& self,
                      const std::vector<Value>& args,
                      const StringFactory& makeString,
                      const ValueStrInvoker& valueStr) const;
    Value methodReplace(Object& self,
                        const std::vector<Value>& args,
                        const StringFactory& makeString,
                        const ValueStrInvoker& valueStr) const;
    Value methodUpper(Object& self,
                      const std::vector<Value>& args,
                      const StringFactory& makeString,
                      const ValueStrInvoker& valueStr) const;
    Value methodLower(Object& self,
                      const std::vector<Value>& args,
                      const StringFactory& makeString,
                      const ValueStrInvoker& valueStr) const;
    Value methodStrip(Object& self,
                      const std::vector<Value>& args,
                      const StringFactory& makeString,
                      const ValueStrInvoker& valueStr) const;
    Value methodStartsWith(Object& self,
                           const std::vector<Value>& args,
                           const StringFactory& makeString,
                           const ValueStrInvoker& valueStr) const;
    Value methodEndsWith(Object& self,
                         const std::vector<Value>& args,
                         const StringFactory& makeString,
                         const ValueStrInvoker& valueStr) const;
    Value methodAt(Object& self,
                   const std::vector<Value>& args,
                   const StringFactory& makeString,
                   const ValueStrInvoker& valueStr) const;

    // Member attributes
    Value memberLengthGet(Object& self) const;
    Value memberLengthSet(Object& self, const Value& value) const;
};

} // namespace gs