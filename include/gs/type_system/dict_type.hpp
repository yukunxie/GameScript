#pragma once

#include "gs/type_system/type_base.hpp"
#include <functional>

namespace gs {

// Hash functor for Value to use as map key
struct ValueHash {
    std::size_t operator()(const Value& v) const {
        // Combine type and payload for hash
        std::size_t h = std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(v.type));
        h ^= std::hash<std::int64_t>{}(v.payload) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// Equality functor for Value
struct ValueEqual {
    bool operator()(const Value& a, const Value& b) const {
        if (a.type != b.type) return false;
        if (a.type == ValueType::Ref) {
            return a.object == b.object;
        }
        return a.payload == b.payload;
    }
};

class DictObject : public Object {
public:
    using MapType = std::unordered_map<Value, Value, ValueHash, ValueEqual>;

    explicit DictObject(const Type& typeRef);
    DictObject(const Type& typeRef, MapType values);
    // Legacy constructor for int-keyed dicts (backward compatibility)
    DictObject(const Type& typeRef, std::unordered_map<std::int64_t, Value> intValues);

    const Type& getType() const override;
    MapType& data();
    const MapType& data() const;

private:
    const Type* type_;
    MapType data_;
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