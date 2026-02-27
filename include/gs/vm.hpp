#pragma once

#include "gs/binding.hpp"
#include "gs/bytecode.hpp"
#include "gs/task_system.hpp"
#include "gs/type_system.hpp"

#include <cstdint>
#include <memory>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gs {

enum class GcGeneration : std::uint8_t {
    Young,
    Old
};

enum class GcPhase : std::uint8_t {
    Idle,
    MinorMark,
    MinorSweep,
    MajorMark,
    MajorSweep
};

struct GcObjectMeta {
    GcGeneration generation{GcGeneration::Young};
    std::uint8_t age{0};
    bool marked{false};
    std::uint32_t regionId{0};
};

struct GcState {
    GcPhase phase{GcPhase::Idle};
    bool requestMajor{false};
    std::size_t allocCountSinceLastCycle{0};
    std::size_t markCursor{0};
    std::size_t sweepCursor{0};
    std::vector<std::uint64_t> markQueue;
    std::vector<std::uint64_t> sweepList;
    std::unordered_set<std::uint64_t> rememberedSet;
    std::size_t minorYoungThreshold{256};
    std::size_t majorObjectThreshold{4096};
    std::size_t promotionAge{2};
    std::size_t sliceBudgetObjects{16};
};

struct Frame {
    std::size_t functionIndex{0};
    std::size_t ip{0};
    std::shared_ptr<const Module> modulePin;
    bool replaceReturnWithInstance{false};
    Value constructorInstance{Value::Nil()};
    std::vector<Value> locals;
    std::vector<Value> captures;
    std::vector<Value> stack;
    std::size_t stackTop{0};
    std::array<Value, 8> registers{Value::Nil(), Value::Nil(), Value::Nil(), Value::Nil(), Value::Nil(), Value::Nil(), Value::Nil(), Value::Nil()};
    Value registerValue{Value::Nil()};
};

struct ExecutionContext {
    std::vector<Frame> frames;
    Value returnValue{Value::Nil()};
    bool deleteHooksRan{false};
    std::shared_ptr<const Module> modulePin;
    std::vector<std::string> stringPool;
    std::unordered_map<const Module*, std::unordered_map<std::string, Value>> moduleRuntimeGlobals;
    std::unordered_map<const Module*, Value> moduleRuntimeObjects;
    std::unordered_set<const Module*> initializedModules;
    std::unordered_set<const Module*> moduleInitInProgress;
    std::unordered_map<std::string, Value> moduleObjectCache;
    std::unordered_map<std::uint64_t, std::unique_ptr<Object>> objectHeap;
    std::unordered_map<std::uint64_t, GcObjectMeta> gcMeta;
    std::unordered_map<Object*, std::uint64_t> objectPtrToId;
    GcState gc;
};

class VirtualMachine {
public:
    VirtualMachine(std::shared_ptr<const Module> module,
                   const HostRegistry& hosts,
                   TaskSystem& tasks);

    Value runFunction(const std::string& functionName, const std::vector<Value>& args = {});
    void ensureModuleInitialized(ExecutionContext& context, const std::shared_ptr<const Module>& modulePin);

private:
    std::size_t findFunctionIndex(const std::string& name) const;
    bool execute(ExecutionContext& context, std::size_t stepBudget = 200);
    static void pushCallFrame(ExecutionContext& ctx,
                              std::shared_ptr<const Module> modulePin,
                              std::size_t functionIndex,
                              const std::vector<Value>& args,
                              bool replaceReturnWithInstance = false,
                              Value constructorInstance = Value::Nil(),
                              std::vector<Value> captures = {});
    void runDeleteHooks(ExecutionContext& context);

    Object& getObject(ExecutionContext& context, const Value& ref);

    std::shared_ptr<const Module> module_;
    const HostRegistry& hosts_;
    TaskSystem& tasks_;
    ListType listType_;
    DictType dictType_;
	StringType stringType_;
    FunctionType functionType_;
    LambdaType lambdaType_;
    NativeFunctionType nativeFunctionType_;
    ClassType classType_;
    ModuleType moduleType_;
    ScriptInstanceType instanceType_;
    UpvalueCellType upvalueCellType_;
};

} // namespace gs
