#pragma once

#include "gs/binding.hpp"
#include "gs/bytecode.hpp"
#include "gs/task_system.hpp"
#include "gs/type_system.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace gs {

enum class RunState {
    Running,
    Suspended,
    Completed
};

struct Frame {
    std::size_t functionIndex{0};
    std::size_t ip{0};
    std::shared_ptr<const Module> modulePin;
    bool replaceReturnWithInstance{false};
    Value constructorInstance{Value::Nil()};
    std::vector<Value> locals;
    std::vector<Value> stack;
};

struct ExecutionContext {
    std::vector<Frame> frames;
    RunState state{RunState::Running};
    std::chrono::steady_clock::time_point wakeTime{};
    Value returnValue{Value::Nil()};
    bool deleteHooksRan{false};
    std::shared_ptr<const Module> modulePin;
    std::vector<std::string> stringPool;
    std::unordered_map<std::uint64_t, std::unique_ptr<Object>> objectHeap;
};

class VirtualMachine {
public:
    VirtualMachine(std::shared_ptr<const Module> module,
                   const HostRegistry& hosts,
                   TaskSystem& tasks);

    Value runFunction(const std::string& functionName, const std::vector<Value>& args = {});
    ExecutionContext beginCoroutine(const std::string& functionName, const std::vector<Value>& args = {});
    RunState resume(ExecutionContext& context, std::size_t stepBudget = 200);

private:
    std::size_t findFunctionIndex(const std::string& name) const;
    static void pushCallFrame(ExecutionContext& ctx,
                              std::shared_ptr<const Module> modulePin,
                              std::size_t functionIndex,
                              const std::vector<Value>& args,
                              bool replaceReturnWithInstance = false,
                              Value constructorInstance = Value::Nil());
    void runDeleteHooks(ExecutionContext& context);

    Object& getObject(ExecutionContext& context, const Value& ref);

    std::shared_ptr<const Module> module_;
    const HostRegistry& hosts_;
    TaskSystem& tasks_;
    ListType listType_;
    DictType dictType_;
    FunctionType functionType_;
    ClassType classType_;
    ModuleType moduleType_;
    ScriptInstanceType instanceType_;
};

} // namespace gs
