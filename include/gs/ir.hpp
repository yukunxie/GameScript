#pragma once

#include "gs/bytecode.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gs {

struct IRInstruction {
    OpCode op{};
    SlotType aSlotType{SlotType::None};
    std::int32_t a{};
    SlotType bSlotType{SlotType::None};
    std::int32_t b{};
    std::size_t line{0};
    std::size_t column{0};
};

struct FunctionIR {
    std::string name;
    std::vector<std::string> params;
    std::vector<IRInstruction> code;
    std::size_t localCount{0};
    std::vector<std::string> localDebugNames;
};

inline int stackDelta(const IRInstruction& instruction) {
    switch (instruction.op) {
    case OpCode::PushConst:
    case OpCode::LoadLocal:
    case OpCode::PushLocal:
    case OpCode::LoadName:
    case OpCode::PushName:
    case OpCode::PushReg:
        return 1;
    case OpCode::StoreName:
    case OpCode::StoreLocal:
    case OpCode::JumpIfFalse:
    case OpCode::Pop:
        return -1;
    case OpCode::JumpIfFalseReg:
        return 0;
    case OpCode::Add:
    case OpCode::Sub:
    case OpCode::Mul:
    case OpCode::Div:
    case OpCode::LessThan:
    case OpCode::GreaterThan:
    case OpCode::Equal:
    case OpCode::NotEqual:
    case OpCode::LessEqual:
    case OpCode::GreaterEqual:
        if (instruction.aSlotType != SlotType::None || instruction.bSlotType != SlotType::None) {
            return 0;
        }
        return -1;
    case OpCode::StoreAttr:
        return -1;
    case OpCode::Negate:
    case OpCode::Not:
        return 0;
    case OpCode::Jump:
    case OpCode::Sleep:
    case OpCode::Yield:
    case OpCode::CallIntrinsic:
        return 0;
    case OpCode::CallHost:
        return 1 - instruction.b;
    case OpCode::CallFunc:
        return -instruction.b;
    case OpCode::NewInstance:
        return -instruction.b;
    case OpCode::LoadAttr:
        return 0;
    case OpCode::CallMethod:
        return -instruction.b;
    case OpCode::CallValue:
        return -instruction.a;
    case OpCode::SpawnFunc:
        return 1 - instruction.b;
    case OpCode::Await:
        return 0;
    case OpCode::MakeList:
        return 1 - instruction.a;
    case OpCode::MakeDict:
        return 1 - (instruction.a * 2);
    case OpCode::Return:
        return -1;
    case OpCode::MoveLocalToReg:
    case OpCode::MoveNameToReg:
    case OpCode::ConstToReg:
    case OpCode::LoadConst:
    case OpCode::StoreLocalFromReg:
    case OpCode::StoreNameFromReg:
        return 0;
    }
    return 0;
}

inline std::size_t estimateStackSlots(const FunctionIR& ir) {
    int current = 0;
    int maximum = 0;
    for (const auto& instruction : ir.code) {
        current += stackDelta(instruction);
        if (current < 0) {
            current = 0;
        }
        maximum = std::max(maximum, current);
    }
    return static_cast<std::size_t>(maximum);
}

inline FunctionBytecode lowerFunctionIR(const FunctionIR& ir) {
    FunctionBytecode out;
    out.name = ir.name;
    out.params = ir.params;
    out.localCount = ir.localCount;
    out.stackSlotCount = estimateStackSlots(ir);
    out.code.reserve(ir.code.size());
    for (const auto& instruction : ir.code) {
        out.code.push_back({instruction.op,
                            instruction.aSlotType,
                            instruction.a,
                            instruction.bSlotType,
                            instruction.b});
    }
    return out;
}

} // namespace gs
