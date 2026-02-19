#include "gs/vm.hpp"

#include <chrono>
#include <atomic>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace gs {

namespace {

Value popValue(std::vector<Value>& stack) {
    if (stack.empty()) {
        throw std::runtime_error("Stack underflow");
    }
    const Value v = stack.back();
    stack.pop_back();
    return v;
}

std::vector<Value> collectArgs(std::vector<Value>& stack, std::size_t count) {
    if (stack.size() < count) {
        throw std::runtime_error("Not enough arguments on stack");
    }
    std::vector<Value> args(count);
    for (std::size_t i = 0; i < count; ++i) {
        args[count - 1 - i] = popValue(stack);
    }
    return args;
}

const std::string& getString(const ExecutionContext& context, const Value& value) {
    const auto idx = static_cast<std::size_t>(value.asStringIndex());
    if (idx >= context.stringPool.size()) {
        throw std::runtime_error("String index out of range");
    }
    return context.stringPool[idx];
}

std::string __str__ValueImpl(const ExecutionContext& context,
                             const Value& value,
                             std::unordered_set<std::uint64_t>& visitingRefs);

std::uint64_t nextGlobalObjectId() {
    static std::atomic<std::uint64_t> nextId{1};
    return nextId.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t toObjectId(const Value& ref) {
    const std::int64_t rawId = ref.asRef();
    if (rawId < 0) {
        throw std::runtime_error("Object reference id is negative");
    }
    return static_cast<std::uint64_t>(rawId);
}

Value makeRefValue(std::uint64_t objectId) {
    constexpr auto kMaxRefId = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (objectId > kMaxRefId) {
        throw std::runtime_error("Object reference id overflow");
    }
    return Value::Ref(static_cast<std::int64_t>(objectId));
}

Value emplaceObject(ExecutionContext& context, std::unique_ptr<Object> object) {
    const std::uint64_t id = nextGlobalObjectId();
    object->setObjectId(id);
    context.objectHeap.emplace(id, std::move(object));
    return makeRefValue(id);
}

std::string __str__RefObject(const ExecutionContext& context,
                             std::uint64_t objectId,
                             std::unordered_set<std::uint64_t>& visitingRefs) {
    auto it = context.objectHeap.find(objectId);
    if (it == context.objectHeap.end() || !it->second) {
        return "ref(" + std::to_string(objectId) + ")";
    }

    if (visitingRefs.contains(objectId)) {
        return "[Circular]";
    }

    visitingRefs.insert(objectId);
    Object& object = *(it->second);
    const auto valueStr = [&](const Value& nested) {
        return __str__ValueImpl(context, nested, visitingRefs);
    };
    std::string out = object.getType().__str__(object, valueStr);

    visitingRefs.erase(objectId);
    return out;
}

std::string __str__ValueImpl(const ExecutionContext& context,
                             const Value& value,
                             std::unordered_set<std::uint64_t>& visitingRefs) {
    switch (value.type) {
    case ValueType::Nil:
        return "nil";
    case ValueType::Int:
        return std::to_string(value.payload);
    case ValueType::String:
        return getString(context, value);
    case ValueType::Ref:
        return __str__RefObject(context, toObjectId(value), visitingRefs);
    case ValueType::Function:
        return "[Function]";
    }
    return "";
}

std::string __str__Value(const ExecutionContext& context, const Value& value) {
    std::unordered_set<std::uint64_t> visitingRefs;
    return __str__ValueImpl(context, value, visitingRefs);
}

std::string typeNameOfValue(const ExecutionContext& context, const Value& value) {
    switch (value.type) {
    case ValueType::Nil:
        return "nil";
    case ValueType::Int:
        return "int";
    case ValueType::String:
        return "string";
    case ValueType::Function:
        return "function";
    case ValueType::Ref:
        break;
    }

    const auto objectId = toObjectId(value);
    auto it = context.objectHeap.find(objectId);
    if (it == context.objectHeap.end() || !it->second) {
        return "ref";
    }

    Object& object = *(it->second);
    if (auto* instance = dynamic_cast<ScriptInstanceObject*>(&object)) {
        return instance->className();
    }
    return object.getType().name();
}

Value makeRuntimeString(ExecutionContext& context, const std::string& text) {
    context.stringPool.push_back(text);
    return Value::String(static_cast<std::int64_t>(context.stringPool.size() - 1));
}

bool valueEquals(const ExecutionContext& context, const Value& lhs, const Value& rhs) {
    if (lhs.type != rhs.type) {
        return false;
    }
    if (lhs.type == ValueType::String) {
        return getString(context, lhs) == getString(context, rhs);
    }
    return lhs.payload == rhs.payload;
}

class VmHostContext final : public HostContext {
public:
    explicit VmHostContext(ExecutionContext& context) : context_(context) {}

    Value createObject(std::unique_ptr<Object> object) override {
        return emplaceObject(context_, std::move(object));
    }

    Value createString(const std::string& text) override {
        return makeRuntimeString(context_, text);
    }

    Object& getObject(const Value& ref) override {
        if (!ref.isRef()) {
            throw std::runtime_error("Host context expects object reference");
        }
        const std::uint64_t objectId = toObjectId(ref);
        auto it = context_.objectHeap.find(objectId);
        if (it == context_.objectHeap.end() || !it->second) {
            throw std::runtime_error("Host object reference not found");
        }
        return *(it->second);
    }

    std::string __str__(const Value& value) override {
        return __str__Value(context_, value);
    }

    std::string typeName(const Value& value) override {
        return typeNameOfValue(context_, value);
    }

    std::uint64_t objectId(const Value& ref) override {
        if (!ref.isRef()) {
            throw std::runtime_error("id() requires object reference");
        }
        return toObjectId(ref);
    }

private:
    ExecutionContext& context_;
};

Value makeFunctionObject(ExecutionContext& context, FunctionType& functionType, std::size_t functionIndex) {
    return emplaceObject(context, std::make_unique<FunctionObject>(functionType, functionIndex));
}

Value normalizeFunctionValue(ExecutionContext& context,
                             FunctionType& functionType,
                             const Value& value) {
    if (!value.isFunction()) {
        return value;
    }
    return makeFunctionObject(context, functionType, static_cast<std::size_t>(value.asFunctionIndex()));
}

void initializeInstanceAttributes(const Module& module,
                                  std::size_t classIndex,
                                  ScriptInstanceObject& instance) {
    if (classIndex >= module.classes.size()) {
        throw std::runtime_error("Class index out of range");
    }

    const auto& cls = module.classes[classIndex];
    if (cls.baseClassIndex >= 0) {
        initializeInstanceAttributes(module, static_cast<std::size_t>(cls.baseClassIndex), instance);
    }

    for (const auto& attr : cls.attributes) {
        instance.fields()[attr.name] = attr.defaultValue;
    }
}

} // namespace

VirtualMachine::VirtualMachine(std::shared_ptr<const Module> module,
                               const HostRegistry& hosts,
                               TaskSystem& tasks)
    : module_(std::move(module)), hosts_(hosts), tasks_(tasks) {
    if (!module_) {
        throw std::runtime_error("Module is null");
    }
}

std::size_t VirtualMachine::findFunctionIndex(const std::string& name) const {
    for (std::size_t i = 0; i < module_->functions.size(); ++i) {
        if (module_->functions[i].name == name) {
            return i;
        }
    }
    throw std::runtime_error("Script function not found: " + name);
}

bool VirtualMachine::tryFindClassMethod(std::size_t classIndex,
                                        const std::string& methodName,
                                        std::size_t& outFunctionIndex) const {
    std::int32_t index = static_cast<std::int32_t>(classIndex);
    while (index >= 0) {
        const auto& cls = module_->classes.at(static_cast<std::size_t>(index));
        for (const auto& method : cls.methods) {
            if (method.name == methodName) {
                outFunctionIndex = method.functionIndex;
                return true;
            }
        }
        index = cls.baseClassIndex;
    }
    return false;
}

void VirtualMachine::pushCallFrame(ExecutionContext& ctx,
                                   const Module& module,
                                   std::size_t functionIndex,
                                   const std::vector<Value>& args) {
    const auto& fn = module.functions.at(functionIndex);
    if (args.size() != fn.params.size()) {
        throw std::runtime_error("Function argument count mismatch: " + fn.name);
    }

    Frame frame;
    frame.functionIndex = functionIndex;
    frame.ip = 0;
    frame.locals.assign(fn.localCount, Value::Nil());
    for (std::size_t i = 0; i < args.size(); ++i) {
        frame.locals[i] = args[i];
    }
    ctx.frames.push_back(std::move(frame));
}

Object& VirtualMachine::getObject(ExecutionContext& context, const Value& ref) {
    if (!ref.isRef()) {
        throw std::runtime_error("Method target is not an object reference");
    }
    auto it = context.objectHeap.find(toObjectId(ref));
    if (it == context.objectHeap.end() || !it->second) {
        throw std::runtime_error("Object reference not found");
    }
    return *(it->second);
}

ExecutionContext VirtualMachine::beginCoroutine(const std::string& functionName,
                                                const std::vector<Value>& args) {
    ExecutionContext context;
    context.modulePin = module_;
    context.stringPool = module_->strings;
    pushCallFrame(context, *module_, findFunctionIndex(functionName), args);
    context.state = RunState::Running;
    return context;
}

RunState VirtualMachine::resume(ExecutionContext& context, std::size_t stepBudget) {
    if (context.state == RunState::Completed) {
        return RunState::Completed;
    }

    const auto now = std::chrono::steady_clock::now();
    if (context.state == RunState::Suspended && now < context.wakeTime) {
        return RunState::Suspended;
    }
    context.state = RunState::Running;

    std::size_t steps = 0;
    while (steps++ < stepBudget) {
        if (context.frames.empty()) {
            context.state = RunState::Completed;
            return RunState::Completed;
        }

        auto& frame = context.frames.back();
        const auto& fn = module_->functions.at(frame.functionIndex);
        if (frame.ip >= fn.code.size()) {
            throw std::runtime_error("Instruction pointer out of range");
        }

        const Instruction ins = fn.code[frame.ip++];
        switch (ins.op) {
        case OpCode::PushConst: {
            Value value = module_->constants.at(ins.a);
            value = normalizeFunctionValue(context, functionType_, value);
            frame.stack.push_back(value);
            break;
        }
        case OpCode::LoadLocal:
            frame.stack.push_back(normalizeFunctionValue(context, functionType_, frame.locals.at(ins.a)));
            break;
        case OpCode::StoreLocal: {
            const Value v = popValue(frame.stack);
            frame.locals.at(ins.a) = v;
            break;
        }
        case OpCode::Add: {
            const Value rhs = popValue(frame.stack);
            const Value lhs = popValue(frame.stack);
            if (lhs.isInt() && rhs.isInt()) {
                frame.stack.push_back(Value::Int(lhs.asInt() + rhs.asInt()));
            } else {
                frame.stack.push_back(makeRuntimeString(context, __str__Value(context, lhs) + __str__Value(context, rhs)));
            }
            break;
        }
        case OpCode::Sub: {
            const Value rhs = popValue(frame.stack);
            const Value lhs = popValue(frame.stack);
            frame.stack.push_back(Value::Int(lhs.asInt() - rhs.asInt()));
            break;
        }
        case OpCode::Mul: {
            const Value rhs = popValue(frame.stack);
            const Value lhs = popValue(frame.stack);
            frame.stack.push_back(Value::Int(lhs.asInt() * rhs.asInt()));
            break;
        }
        case OpCode::Div: {
            const Value rhs = popValue(frame.stack);
            const Value lhs = popValue(frame.stack);
            frame.stack.push_back(Value::Int(lhs.asInt() / rhs.asInt()));
            break;
        }
        case OpCode::LessThan: {
            const Value rhs = popValue(frame.stack);
            const Value lhs = popValue(frame.stack);
            frame.stack.push_back(Value::Int(lhs.asInt() < rhs.asInt() ? 1 : 0));
            break;
        }
        case OpCode::GreaterThan: {
            const Value rhs = popValue(frame.stack);
            const Value lhs = popValue(frame.stack);
            frame.stack.push_back(Value::Int(lhs.asInt() > rhs.asInt() ? 1 : 0));
            break;
        }
        case OpCode::Equal: {
            const Value rhs = popValue(frame.stack);
            const Value lhs = popValue(frame.stack);
            frame.stack.push_back(Value::Int(valueEquals(context, lhs, rhs) ? 1 : 0));
            break;
        }
        case OpCode::NotEqual: {
            const Value rhs = popValue(frame.stack);
            const Value lhs = popValue(frame.stack);
            frame.stack.push_back(Value::Int(valueEquals(context, lhs, rhs) ? 0 : 1));
            break;
        }
        case OpCode::LessEqual: {
            const Value rhs = popValue(frame.stack);
            const Value lhs = popValue(frame.stack);
            frame.stack.push_back(Value::Int(lhs.asInt() <= rhs.asInt() ? 1 : 0));
            break;
        }
        case OpCode::GreaterEqual: {
            const Value rhs = popValue(frame.stack);
            const Value lhs = popValue(frame.stack);
            frame.stack.push_back(Value::Int(lhs.asInt() >= rhs.asInt() ? 1 : 0));
            break;
        }
        case OpCode::Jump:
            frame.ip = static_cast<std::size_t>(ins.a);
            break;
        case OpCode::JumpIfFalse: {
            const Value cond = popValue(frame.stack);
            if (cond.asInt() == 0) {
                frame.ip = static_cast<std::size_t>(ins.a);
            }
            break;
        }
        case OpCode::CallHost: {
            const auto args = collectArgs(frame.stack, static_cast<std::size_t>(ins.b));
            const auto& name = module_->strings.at(ins.a);
            VmHostContext hostContext(context);
            frame.stack.push_back(hosts_.invoke(name, hostContext, args));
            break;
        }
        case OpCode::CallFunc: {
            const auto args = collectArgs(frame.stack, static_cast<std::size_t>(ins.b));
            pushCallFrame(context, *module_, static_cast<std::size_t>(ins.a), args);
            break;
        }
        case OpCode::NewInstance: {
            const auto ctorArgs = collectArgs(frame.stack, static_cast<std::size_t>(ins.b));
            if (!ctorArgs.empty()) {
                throw std::runtime_error("Class constructor arguments are not supported yet");
            }
            const std::size_t classIdx = static_cast<std::size_t>(ins.a);
            if (classIdx >= module_->classes.size()) {
                throw std::runtime_error("Class index out of range");
            }
            const std::string& className = module_->classes[classIdx].name;
            const Value instanceRef = emplaceObject(context,
                                                    std::make_unique<ScriptInstanceObject>(instanceType_, classIdx, className));
            auto* instance = dynamic_cast<ScriptInstanceObject*>(&getObject(context, instanceRef));
            if (!instance) {
                throw std::runtime_error("Failed to create script instance");
            }
            initializeInstanceAttributes(*module_, classIdx, *instance);
            frame.stack.push_back(instanceRef);
            break;
        }
        case OpCode::LoadAttr: {
            const Value selfRef = popValue(frame.stack);
            Object& object = getObject(context, selfRef);
            const auto& attrName = module_->strings.at(ins.a);
            if (auto* instance = dynamic_cast<ScriptInstanceObject*>(&object)) {
                auto it = instance->fields().find(attrName);
                if (it == instance->fields().end()) {
                    throw std::runtime_error("Unknown class attribute: " + attrName);
                }
                it->second = normalizeFunctionValue(context, functionType_, it->second);
                frame.stack.push_back(it->second);
            } else {
                frame.stack.push_back(object.getType().getMember(object, attrName));
            }
            break;
        }
        case OpCode::StoreAttr: {
            const Value assigned = popValue(frame.stack);
            const Value selfRef = popValue(frame.stack);
            Object& object = getObject(context, selfRef);
            const auto& attrName = module_->strings.at(ins.a);
            const Value normalized = normalizeFunctionValue(context, functionType_, assigned);
            if (auto* instance = dynamic_cast<ScriptInstanceObject*>(&object)) {
                instance->fields()[attrName] = normalized;
                frame.stack.push_back(normalized);
            } else {
                frame.stack.push_back(object.getType().setMember(object, attrName, normalized));
            }
            break;
        }
        case OpCode::CallMethod: {
            const auto args = collectArgs(frame.stack, static_cast<std::size_t>(ins.b));
            const Value selfRef = popValue(frame.stack);
            Object& object = getObject(context, selfRef);
            const auto& methodName = module_->strings.at(ins.a);

            const auto makeString = [&](const std::string& text) {
                return makeRuntimeString(context, text);
            };
            const auto valueStr = [&](const Value& nested) {
                return __str__Value(context, nested);
            };

            if (auto* instance = dynamic_cast<ScriptInstanceObject*>(&object)) {
                auto fieldIt = instance->fields().find(methodName);
                if (fieldIt != instance->fields().end()) {
                    const Value callable = normalizeFunctionValue(context, functionType_, fieldIt->second);
                    fieldIt->second = callable;
                    if (!callable.isRef()) {
                        throw std::runtime_error("Object property is not callable: " + methodName);
                    }

                    Object& callableObject = getObject(context, callable);
                    auto* fnObject = dynamic_cast<FunctionObject*>(&callableObject);
                    if (!fnObject) {
                        throw std::runtime_error("Object property is not function object: " + methodName);
                    }

                    std::vector<Value> methodArgs;
                    methodArgs.reserve(args.size() + 1);
                    methodArgs.push_back(selfRef);
                    methodArgs.insert(methodArgs.end(), args.begin(), args.end());
                    pushCallFrame(context,
                                  *module_,
                                  fnObject->functionIndex(),
                                  methodArgs);
                    break;
                }

                std::size_t classMethodIndex = 0;
                if (tryFindClassMethod(instance->classIndex(), methodName, classMethodIndex)) {
                    std::vector<Value> methodArgs;
                    methodArgs.reserve(args.size() + 1);
                    methodArgs.push_back(selfRef);
                    methodArgs.insert(methodArgs.end(), args.begin(), args.end());
                    pushCallFrame(context,
                                  *module_,
                                  classMethodIndex,
                                  methodArgs);
                    break;
                }

                frame.stack.push_back(object.getType().callMethod(object,
                                                                  methodName,
                                                                  args,
                                                                  makeString,
                                                                  valueStr));
            } else {
                frame.stack.push_back(object.getType().callMethod(object,
                                                                  methodName,
                                                                  args,
                                                                  makeString,
                                                                  valueStr));
            }
            break;
        }
        case OpCode::CallValue: {
            const auto args = collectArgs(frame.stack, static_cast<std::size_t>(ins.a));
            Value callable = popValue(frame.stack);
            callable = normalizeFunctionValue(context, functionType_, callable);
            if (!callable.isRef()) {
                throw std::runtime_error("Attempted to call a non-function value");
            }

            Object& callableObject = getObject(context, callable);
            auto* fnObject = dynamic_cast<FunctionObject*>(&callableObject);
            if (!fnObject) {
                throw std::runtime_error("Attempted to call a non-function object");
            }
            pushCallFrame(context,
                          *module_,
                          fnObject->functionIndex(),
                          args);
            break;
        }
        case OpCode::CallIntrinsic:
            throw std::runtime_error("CallIntrinsic is deprecated. Use Type exported methods.");
        case OpCode::SpawnFunc: {
            const auto args = collectArgs(frame.stack, static_cast<std::size_t>(ins.b));
            const auto funcName = module_->functions.at(ins.a).name;
            const auto module = module_;
            const auto* hosts = &hosts_;
            auto& tasks = tasks_;
            const std::int64_t handle = tasks_.enqueue([module, hosts, &tasks, funcName, args]() {
                VirtualMachine vm(module, *hosts, tasks);
                return vm.runFunction(funcName, args);
            });
            frame.stack.push_back(Value::Int(handle));
            break;
        }
        case OpCode::Await: {
            const auto handle = popValue(frame.stack).asInt();
            frame.stack.push_back(tasks_.await(handle));
            break;
        }
        case OpCode::MakeList: {
            const std::size_t count = static_cast<std::size_t>(ins.a);
            const auto elements = collectArgs(frame.stack, count);
            frame.stack.push_back(emplaceObject(context, std::make_unique<ListObject>(listType_, elements)));
            break;
        }
        case OpCode::MakeDict: {
            const std::size_t pairCount = static_cast<std::size_t>(ins.a);
            if (frame.stack.size() < pairCount * 2) {
                throw std::runtime_error("Not enough stack values for dict literal");
            }
            std::unordered_map<std::int64_t, Value> values;
            for (std::size_t i = 0; i < pairCount; ++i) {
                const Value value = popValue(frame.stack);
                const Value key = popValue(frame.stack);
                values[key.asInt()] = value;
            }
            frame.stack.push_back(emplaceObject(context, std::make_unique<DictObject>(dictType_, std::move(values))));
            break;
        }
        case OpCode::Sleep:
            context.state = RunState::Suspended;
            context.wakeTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(ins.a);
            return RunState::Suspended;
        case OpCode::Yield:
            context.state = RunState::Suspended;
            context.wakeTime = std::chrono::steady_clock::now();
            return RunState::Suspended;
        case OpCode::Return: {
            const Value ret = frame.stack.empty() ? Value::Nil() : popValue(frame.stack);
            context.frames.pop_back();
            if (context.frames.empty()) {
                context.returnValue = ret;
                context.state = RunState::Completed;
                return RunState::Completed;
            }
            context.frames.back().stack.push_back(ret);
            break;
        }
        case OpCode::Pop:
            (void)popValue(frame.stack);
            break;
        }
    }

    return context.state;
}

Value VirtualMachine::runFunction(const std::string& functionName, const std::vector<Value>& args) {
    ExecutionContext ctx = beginCoroutine(functionName, args);
    while (true) {
        const auto state = resume(ctx, 1000);
        if (state == RunState::Completed) {
            return ctx.returnValue;
        }
        if (state == RunState::Suspended) {
            const auto now = std::chrono::steady_clock::now();
            if (ctx.wakeTime > now) {
                std::this_thread::sleep_for(ctx.wakeTime - now);
            }
        }
    }
}

} // namespace gs
