#pragma once

#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace gs {

class Object;

enum class ValueType : std::uint8_t {
    Nil,
    Int,
    String,
    Ref,
    Function,
    Class,
    Module
};

struct Value {
    ValueType type{ValueType::Nil};
    union {
        std::int64_t payload;
        Object* object;
    };

    Value() : type(ValueType::Nil), payload(0) {}

    static Value Nil() { return {}; }
    static Value Int(std::int64_t v) {
        Value out;
        out.type = ValueType::Int;
        out.payload = v;
        return out;
    }
    static Value String(std::int64_t index) {
        Value out;
        out.type = ValueType::String;
        out.payload = index;
        return out;
    }
    static Value Ref(Object* ptr) {
        Value out;
        out.type = ValueType::Ref;
        out.object = ptr;
        return out;
    }
    static Value Function(std::int64_t functionIndex) {
        Value out;
        out.type = ValueType::Function;
        out.payload = functionIndex;
        return out;
    }
    static Value Class(std::int64_t classIndex) {
        Value out;
        out.type = ValueType::Class;
        out.payload = classIndex;
        return out;
    }
    static Value Module(std::int64_t moduleIndex) {
        Value out;
        out.type = ValueType::Module;
        out.payload = moduleIndex;
        return out;
    }

    bool isInt() const { return type == ValueType::Int; }
    bool isString() const { return type == ValueType::String; }
    bool isRef() const { return type == ValueType::Ref; }
    bool isFunction() const { return type == ValueType::Function; }
    bool isClass() const { return type == ValueType::Class; }
    bool isModule() const { return type == ValueType::Module; }
    bool isNil() const { return type == ValueType::Nil; }

    std::int64_t asInt() const {
        if (!isInt()) {
            throw std::runtime_error("Value is not integer");
        }
        return payload;
    }

    Object* asRef() const {
        if (!isRef()) {
            throw std::runtime_error("Value is not reference");
        }
        return object;
    }

    std::int64_t asStringIndex() const {
        if (!isString()) {
            throw std::runtime_error("Value is not string");
        }
        return payload;
    }

    std::int64_t asFunctionIndex() const {
        if (!isFunction()) {
            throw std::runtime_error("Value is not function");
        }
        return payload;
    }

    std::int64_t asClassIndex() const {
        if (!isClass()) {
            throw std::runtime_error("Value is not class");
        }
        return payload;
    }

    std::int64_t asModuleIndex() const {
        if (!isModule()) {
            throw std::runtime_error("Value is not module");
        }
        return payload;
    }
};

inline std::ostream& operator<<(std::ostream& os, const Value& value) {
    switch (value.type) {
    case ValueType::Nil:
        os << "nil";
        break;
    case ValueType::Int:
        os << value.payload;
        break;
    case ValueType::String:
        os << "str(" << value.payload << ')';
        break;
    case ValueType::Ref:
        os << "ref(" << static_cast<const void*>(value.object) << ')';
        break;
    case ValueType::Function:
        os << "fn(" << value.payload << ')';
        break;
    case ValueType::Class:
        os << "class(" << value.payload << ')';
        break;
    case ValueType::Module:
        os << "module(" << value.payload << ')';
        break;
    }
    return os;
}

enum class OpCode : std::uint8_t {
    PushConst,
    LoadLocal,
    LoadName,
    StoreLocal,
    Add,
    Sub,
    Mul,
    Div,
    LessThan,
    GreaterThan,
    Equal,
    NotEqual,
    LessEqual,
    GreaterEqual,
    Jump,
    JumpIfFalse,
    CallHost,
    CallFunc,
    NewInstance,
    LoadAttr,
    StoreAttr,
    CallMethod,
    CallValue,
    CallIntrinsic,
    SpawnFunc,
    Await,
    MakeList,
    MakeDict,
    Sleep,
    Yield,
    Return,
    Pop
};

struct Instruction {
    OpCode op{};
    std::int32_t a{};
    std::int32_t b{};
};

struct FunctionBytecode {
    std::string name;
    std::vector<std::string> params;
    std::vector<Instruction> code;
    std::size_t localCount{0};
};

struct ClassMethodBinding {
    std::string name;
    std::size_t functionIndex{0};
};

struct ClassAttributeBinding {
    std::string name;
    Value defaultValue{Value::Nil()};
};

struct ClassBytecode {
    std::string name;
    std::int32_t baseClassIndex{-1};
    std::vector<ClassAttributeBinding> attributes;
    std::vector<ClassMethodBinding> methods;
};

struct GlobalBinding {
    std::string name;
    Value initialValue{Value::Nil()};
};

struct Module {
    std::vector<Value> constants;
    std::vector<std::string> strings;
    std::vector<FunctionBytecode> functions;
    std::vector<ClassBytecode> classes;
    std::vector<GlobalBinding> globals;
};

} // namespace gs
