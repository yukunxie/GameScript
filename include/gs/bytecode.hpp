#pragma once

#include <cstdint>
#include <cstring>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace gs {

class Object;

enum class ValueType : std::uint8_t {
    Nil,
    Int,
    Float,
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
    static Value Float(double v) {
        Value out;
        out.type = ValueType::Float;
        static_assert(sizeof(double) == sizeof(std::int64_t), "double size must match int64 payload storage");
        std::int64_t bits = 0;
        std::memcpy(&bits, &v, sizeof(double));
        out.payload = bits;
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
    bool isFloat() const { return type == ValueType::Float; }
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

    double asFloat() const {
        if (!isFloat()) {
            throw std::runtime_error("Value is not float");
        }
        double out = 0.0;
        std::memcpy(&out, &payload, sizeof(double));
        return out;
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
    case ValueType::Float:
        os << value.asFloat();
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
    StoreName,
    StoreLocal,
    Add,
    Sub,
    Mul,
    Div,
    FloorDiv,
    Mod,
    Pow,
    LessThan,
    GreaterThan,
    Equal,
    NotEqual,
    LessEqual,
    GreaterEqual,
    Is,
    IsNot,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    BitwiseNot,
    ShiftLeft,
    ShiftRight,
    LogicalAnd,
    LogicalOr,
    In,
    NotIn,
    Negate,
    Not,
    Jump,
    JumpIfFalse,
    JumpIfFalseReg,
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
    Pop,
    MoveLocalToReg,
    MoveNameToReg,
    ConstToReg,
    LoadConst,
    PushReg,
    CaptureLocal,
    PushCapture,
    LoadCapture,
    StoreCapture,
    MakeClosure,
    StoreLocalFromReg,
    StoreNameFromReg,
    PushLocal,
    PushName
};

//struct Instruction {
//    OpCode op{};
//    std::int32_t a{};
//    std::int32_t b{};
//};

enum SlotType : std::uint8_t {
    None,
    Local,
    Constant,
    Register,
    // Closure-captured variable slot (index into frame.captures).
    // The capture points to an UpvalueCellObject so reads/writes are by-reference.
    UpValue
};

struct Instruction {
    OpCode op{};
    SlotType aSlotType = SlotType::None;
    std::int32_t a : 24 = -1;
    SlotType bSlotType = SlotType::None;
    std::int32_t b : 24 = -1;
};

struct FunctionBytecode {
    std::string name;
    std::vector<std::string> params;
    std::vector<Instruction> code;
    std::size_t localCount{0};
    std::size_t stackSlotCount{0};
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
