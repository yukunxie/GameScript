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
    std::vector<Value> locals;
    std::vector<Value> stack;
};

struct ExecutionContext {
    std::vector<Frame> frames;
    RunState state{RunState::Running};
    std::chrono::steady_clock::time_point wakeTime{};
    Value returnValue{Value::Nil()};
    std::shared_ptr<const Module> modulePin;
    std::vector<std::string> stringPool;
    std::unordered_map<std::int64_t, std::unique_ptr<Object>> objectHeap;
    std::int64_t nextObjectId{1};
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
                              const Module& module,
                              std::size_t functionIndex,
                              const std::vector<Value>& args);
    std::size_t findClassMethod(std::size_t classIndex, const std::string& methodName) const;

    Object& getObject(ExecutionContext& context, const Value& ref);

    std::shared_ptr<const Module> module_;
    const HostRegistry& hosts_;
    TaskSystem& tasks_;
    ListType listType_;
    DictType dictType_;
    FunctionType functionType_;
    ScriptInstanceType instanceType_;
};

} // namespace gs
