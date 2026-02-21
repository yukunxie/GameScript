#include "gs/vm.hpp"

#include <chrono>
#include <algorithm>
#include <atomic>
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

constexpr std::uint32_t kRegionSpanObjects = 256;

bool tryGetObjectId(const ExecutionContext& context, Object* object, std::uint64_t& outId) {
    if (!object) {
        return false;
    }
    auto it = context.objectPtrToId.find(object);
    if (it == context.objectPtrToId.end()) {
        return false;
    }
    outId = it->second;
    return true;
}

std::size_t countYoungObjects(const ExecutionContext& context) {
    std::size_t count = 0;
    for (const auto& [id, meta] : context.gcMeta) {
        (void)id;
        if (meta.generation == GcGeneration::Young) {
            ++count;
        }
    }
    return count;
}

bool markObjectId(ExecutionContext& context,
                  std::uint64_t objectId,
                  bool youngOnly,
                  bool forceQueue) {
    auto metaIt = context.gcMeta.find(objectId);
    if (metaIt == context.gcMeta.end()) {
        return false;
    }

    auto& meta = metaIt->second;
    if (youngOnly && meta.generation == GcGeneration::Old && !forceQueue) {
        return false;
    }
    if (meta.marked && !forceQueue) {
        return false;
    }

    meta.marked = true;
    context.gc.markQueue.push_back(objectId);
    return true;
}

void markValue(ExecutionContext& context, const Value& value, bool youngOnly) {
    if (!value.isRef()) {
        return;
    }

    std::uint64_t objectId = 0;
    if (!tryGetObjectId(context, value.asRef(), objectId)) {
        return;
    }

    (void)markObjectId(context, objectId, youngOnly, false);
}

void traceObjectChildren(ExecutionContext& context, std::uint64_t objectId, bool youngOnly) {
    auto objectIt = context.objectHeap.find(objectId);
    if (objectIt == context.objectHeap.end() || !objectIt->second) {
        return;
    }

    Object* object = objectIt->second.get();
    if (!object) {
        return;
    }

    const auto markChild = [&](const Value& value) {
        markValue(context, value, youngOnly);
    };

    if (auto* list = dynamic_cast<ListObject*>(object)) {
        for (const auto& value : list->data()) {
            markChild(value);
        }
        return;
    }

    if (auto* dict = dynamic_cast<DictObject*>(object)) {
        for (const auto& [key, value] : dict->data()) {
            (void)key;
            markChild(value);
        }
        return;
    }

    if (auto* instance = dynamic_cast<ScriptInstanceObject*>(object)) {
        for (const auto& [name, value] : instance->fields()) {
            (void)name;
            markChild(value);
        }
        return;
    }

    if (auto* moduleObject = dynamic_cast<ModuleObject*>(object)) {
        for (const auto& [name, value] : moduleObject->exports()) {
            (void)name;
            markChild(value);
        }
        return;
    }
}

void markRoots(ExecutionContext& context, bool youngOnly) {
    for (const auto& frame : context.frames) {
        markValue(context, frame.constructorInstance, youngOnly);
        for (const auto& value : frame.locals) {
            markValue(context, value, youngOnly);
        }
        for (const auto& value : frame.stack) {
            markValue(context, value, youngOnly);
        }
    }
    markValue(context, context.returnValue, youngOnly);

    if (youngOnly) {
        for (const auto objectId : context.gc.rememberedSet) {
            (void)markObjectId(context, objectId, false, true);
        }
    }
}

void beginMinorGc(ExecutionContext& context) {
    context.gc.phase = GcPhase::MinorMark;
    context.gc.markQueue.clear();
    context.gc.sweepList.clear();
    context.gc.markCursor = 0;
    context.gc.sweepCursor = 0;

    for (auto& [id, meta] : context.gcMeta) {
        (void)id;
        if (meta.generation == GcGeneration::Young) {
            meta.marked = false;
        }
    }

    markRoots(context, true);
}

void beginMajorGc(ExecutionContext& context) {
    context.gc.phase = GcPhase::MajorMark;
    context.gc.markQueue.clear();
    context.gc.sweepList.clear();
    context.gc.markCursor = 0;
    context.gc.sweepCursor = 0;

    for (auto& [id, meta] : context.gcMeta) {
        (void)id;
        meta.marked = false;
    }

    markRoots(context, false);
}

void maybeStartGcCycle(ExecutionContext& context) {
    if (context.gc.phase != GcPhase::Idle) {
        return;
    }

    if (context.gc.requestMajor || context.objectHeap.size() >= context.gc.majorObjectThreshold) {
        context.gc.requestMajor = false;
        beginMajorGc(context);
        return;
    }

    if (countYoungObjects(context) >= context.gc.minorYoungThreshold) {
        beginMinorGc(context);
    }
}

void prepareSweepList(ExecutionContext& context, bool youngOnly) {
    context.gc.sweepList.clear();
    context.gc.sweepList.reserve(context.gcMeta.size());
    for (const auto& [id, meta] : context.gcMeta) {
        if (youngOnly && meta.generation != GcGeneration::Young) {
            continue;
        }
        context.gc.sweepList.push_back(id);
    }
    context.gc.sweepCursor = 0;
}

void finishGcCycle(ExecutionContext& context) {
    context.gc.phase = GcPhase::Idle;
    context.gc.markQueue.clear();
    context.gc.sweepList.clear();
    context.gc.markCursor = 0;
    context.gc.sweepCursor = 0;
    context.gc.allocCountSinceLastCycle = 0;
}

void runGcSlice(ExecutionContext& context, std::size_t budgetObjects) {
    maybeStartGcCycle(context);
    if (context.gc.phase == GcPhase::Idle) {
        return;
    }

    std::size_t budget = budgetObjects == 0 ? 1 : budgetObjects;
    while (budget > 0) {
        if (context.gc.phase == GcPhase::MinorMark || context.gc.phase == GcPhase::MajorMark) {
            if (!context.gc.markQueue.empty()) {
                const std::uint64_t objectId = context.gc.markQueue.back();
                context.gc.markQueue.pop_back();
                const bool youngOnly = context.gc.phase == GcPhase::MinorMark;
                traceObjectChildren(context, objectId, youngOnly);
                --budget;
                continue;
            }

            const bool youngOnly = context.gc.phase == GcPhase::MinorMark;
            prepareSweepList(context, youngOnly);
            context.gc.phase = youngOnly ? GcPhase::MinorSweep : GcPhase::MajorSweep;
            continue;
        }

        if (context.gc.phase == GcPhase::MinorSweep || context.gc.phase == GcPhase::MajorSweep) {
            const bool youngOnly = context.gc.phase == GcPhase::MinorSweep;
            if (context.gc.sweepCursor >= context.gc.sweepList.size()) {
                finishGcCycle(context);
                break;
            }

            const std::uint64_t objectId = context.gc.sweepList[context.gc.sweepCursor++];
            auto metaIt = context.gcMeta.find(objectId);
            if (metaIt == context.gcMeta.end()) {
                continue;
            }

            auto& meta = metaIt->second;
            if (youngOnly && meta.generation != GcGeneration::Young) {
                continue;
            }

            if (!meta.marked) {
                auto objectIt = context.objectHeap.find(objectId);
                if (objectIt != context.objectHeap.end() && objectIt->second) {
                    if (!context.deleteHooksRan && dynamic_cast<ScriptInstanceObject*>(objectIt->second.get())) {
                        --budget;
                        continue;
                    }
                    context.objectPtrToId.erase(objectIt->second.get());
                    context.objectHeap.erase(objectIt);
                }
                context.gcMeta.erase(metaIt);
                context.gc.rememberedSet.erase(objectId);
            } else {
                if (youngOnly && meta.generation == GcGeneration::Young) {
                    ++meta.age;
                    if (meta.age >= context.gc.promotionAge) {
                        meta.generation = GcGeneration::Old;
                    }
                }
                meta.marked = false;
            }

            --budget;
            continue;
        }

        break;
    }
}

void runGcUntilIdle(ExecutionContext& context) {
    if (context.gc.phase == GcPhase::Idle) {
        return;
    }

    const std::size_t budget = std::max<std::size_t>(1, context.objectHeap.size() + context.gcMeta.size());
    std::size_t guard = 0;
    while (context.gc.phase != GcPhase::Idle) {
        runGcSlice(context, budget);
        ++guard;
        if (guard > 8192) {
            throw std::runtime_error("Manual GC did not converge");
        }
    }
}

Value collectGarbageNow(ExecutionContext& context, std::int64_t generation) {
    if (generation != 0 && generation != 1) {
        throw std::runtime_error("system.gc(generation): generation must be 0 (minor) or 1 (major)");
    }

    runGcUntilIdle(context);

    const std::size_t before = context.objectHeap.size();
    if (generation == 0) {
        beginMinorGc(context);
    } else {
        beginMajorGc(context);
    }

    runGcUntilIdle(context);

    const std::size_t after = context.objectHeap.size();
    const std::size_t reclaimed = before > after ? (before - after) : 0;
    return Value::Int(static_cast<std::int64_t>(reclaimed));
}

void rememberWriteBarrier(ExecutionContext& context, Object& owner, const Value& assigned) {
    if (!assigned.isRef()) {
        return;
    }

    std::uint64_t ownerId = 0;
    std::uint64_t targetId = 0;
    if (!tryGetObjectId(context, &owner, ownerId)) {
        return;
    }
    if (!tryGetObjectId(context, assigned.asRef(), targetId)) {
        return;
    }

    auto ownerMetaIt = context.gcMeta.find(ownerId);
    auto targetMetaIt = context.gcMeta.find(targetId);
    if (ownerMetaIt == context.gcMeta.end() || targetMetaIt == context.gcMeta.end()) {
        return;
    }

    if (ownerMetaIt->second.generation == GcGeneration::Old &&
        targetMetaIt->second.generation == GcGeneration::Young) {
        context.gc.rememberedSet.insert(ownerId);
    }
}

void registerAllocatedObject(ExecutionContext& context, std::uint64_t id, Object* object) {
    context.objectPtrToId[object] = id;

    GcObjectMeta meta;
    meta.regionId = static_cast<std::uint32_t>(id / kRegionSpanObjects);
    context.gcMeta[id] = meta;

    ++context.gc.allocCountSinceLastCycle;
    if (context.objectHeap.size() >= context.gc.majorObjectThreshold) {
        context.gc.requestMajor = true;
    }
}

Value emplaceObject(ExecutionContext& context, std::unique_ptr<Object> object) {
    const std::uint64_t id = nextGlobalObjectId();
    object->setObjectId(id);
    Object* rawObject = object.get();
    context.objectHeap.emplace(id, std::move(object));
    registerAllocatedObject(context, id, rawObject);
    return Value::Ref(rawObject);
}

std::string __str__RefObject(const ExecutionContext& context,
                             Object* object,
                             std::unordered_set<std::uint64_t>& visitingRefs) {
    if (!object) {
        return "ref(null)";
    }

    const std::uint64_t objectId = object->objectId();

    if (visitingRefs.contains(objectId)) {
        return "[Circular]";
    }

    visitingRefs.insert(objectId);
    const auto valueStr = [&](const Value& nested) {
        return __str__ValueImpl(context, nested, visitingRefs);
    };
    std::string out = object->getType().__str__(*object, valueStr);

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
        return std::to_string(value.asInt());
    case ValueType::String:
        return getString(context, value);
    case ValueType::Ref:
        return __str__RefObject(context, value.asRef(), visitingRefs);
    case ValueType::Function:
        return "[Function]";
    case ValueType::Class:
        return "[Class]";
    case ValueType::Module:
        return "[Module]";
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
    case ValueType::Class:
        return "class";
    case ValueType::Module:
        return "module";
    case ValueType::Ref:
        break;
    }

    Object* object = value.asRef();
    if (!object) {
        return "ref";
    }

    if (auto* instance = dynamic_cast<ScriptInstanceObject*>(object)) {
        return instance->className();
    }
    return object->getType().name();
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
    if (lhs.type == ValueType::Ref) {
        return lhs.asRef() == rhs.asRef();
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
        Object* object = ref.asRef();
        if (!object) {
            throw std::runtime_error("Host object reference not found");
        }
        if (!context_.objectPtrToId.contains(object)) {
            throw std::runtime_error("Host object reference is stale");
        }
        return *object;
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
        Object* object = ref.asRef();
        if (!object) {
            throw std::runtime_error("id() requires valid object reference");
        }
        return object->objectId();
    }

    Value collectGarbage(std::int64_t generation) override {
        return collectGarbageNow(context_, generation);
    }

private:
    ExecutionContext& context_;
};

Value makeFunctionObject(ExecutionContext& context,
                         FunctionType& functionType,
                         std::size_t functionIndex,
                         std::shared_ptr<const Module> modulePin = nullptr) {
    return emplaceObject(context, std::make_unique<FunctionObject>(functionType, functionIndex, std::move(modulePin)));
}

Value makeClassObjectValue(ExecutionContext& context,
                           ClassType& classType,
                           const std::shared_ptr<const Module>& modulePin,
                           std::size_t classIndex) {
    if (!modulePin) {
        throw std::runtime_error("Class value is missing module binding");
    }
    if (classIndex >= modulePin->classes.size()) {
        throw std::runtime_error("Class index out of range");
    }

    return emplaceObject(context,
                         std::make_unique<ClassObject>(classType,
                                                       modulePin->classes[classIndex].name,
                                                       classIndex,
                                                       modulePin));
}

Value makeModuleObjectValue(ExecutionContext& context,
                            ModuleType& moduleType,
                            const std::shared_ptr<const Module>& modulePin,
                            std::size_t moduleNameIndex) {
    if (!modulePin) {
        throw std::runtime_error("Module value is missing module binding");
    }
    if (moduleNameIndex >= modulePin->strings.size()) {
        throw std::runtime_error("Module string index out of range");
    }

    const std::string& moduleName = modulePin->strings[moduleNameIndex];
    return emplaceObject(context,
                         std::make_unique<ModuleObject>(moduleType,
                                                        moduleName,
                                                        modulePin));
}

Value normalizeRuntimeValue(ExecutionContext& context,
                            FunctionType& functionType,
                            ClassType& classType,
                            NativeFunctionType& nativeFunctionType,
                            ModuleType& moduleType,
                            const HostRegistry& hosts,
                            const std::shared_ptr<const Module>& modulePin,
                            const Value& value,
                            bool normalizeStringLiterals) {
    if (value.isFunction()) {
        return makeFunctionObject(context,
                                  functionType,
                                  static_cast<std::size_t>(value.asFunctionIndex()),
                                  modulePin);
    }

    if (value.isClass()) {
        return makeClassObjectValue(context,
                                    classType,
                                    modulePin,
                                    static_cast<std::size_t>(value.asClassIndex()));
    }

    if (value.isModule()) {
        return makeModuleObjectValue(context,
                                     moduleType,
                                     modulePin,
                                     static_cast<std::size_t>(value.asModuleIndex()));
    }

    if (value.isRef() && value.asRef() != nullptr) {
        return value;
    }

    if (normalizeStringLiterals && value.isString() && modulePin) {
        const auto stringIndex = static_cast<std::size_t>(value.asStringIndex());
        if (stringIndex >= modulePin->strings.size()) {
            throw std::runtime_error("String index out of range");
        }
        return makeRuntimeString(context, modulePin->strings[stringIndex]);
    }

    return value;
}

Value resolveRuntimeName(ExecutionContext& context,
                         FunctionType& functionType,
                         ClassType& classType,
                         NativeFunctionType& nativeFunctionType,
                         ModuleType& moduleType,
                         const HostRegistry& hosts,
                         const std::shared_ptr<const Module>& frameModule,
                         const std::string& name) {
    if (!frameModule) {
        throw std::runtime_error("Frame module is null");
    }

    for (const auto& global : frameModule->globals) {
        if (global.name == name) {
            return normalizeRuntimeValue(context,
                                         functionType,
                                         classType,
                                         nativeFunctionType,
                                         moduleType,
                                         hosts,
                                         frameModule,
                                         global.initialValue,
                                         true);
        }
    }

    for (std::size_t i = 0; i < frameModule->functions.size(); ++i) {
        if (frameModule->functions[i].name == name) {
            return makeFunctionObject(context, functionType, i, frameModule);
        }
    }

    for (std::size_t i = 0; i < frameModule->classes.size(); ++i) {
        if (frameModule->classes[i].name == name) {
            return makeClassObjectValue(context, classType, frameModule, i);
        }
    }

    if (hosts.has(name)) {
        return emplaceObject(context,
                             std::make_unique<NativeFunctionObject>(nativeFunctionType,
                                                                    name,
                                                                    [name, hostsPtr = &hosts](HostContext& hostContext, const std::vector<Value>& callArgs) -> Value {
                                                                        return hostsPtr->invoke(name, hostContext, callArgs);
                                                                    }));
    }

    throw std::runtime_error("Undefined symbol: " + name);
}

Object& getObjectFromHeap(ExecutionContext& context, const Value& ref) {
    if (!ref.isRef()) {
        throw std::runtime_error("Method target is not an object reference");
    }
    Object* object = ref.asRef();
    if (!object) {
        throw std::runtime_error("Object reference not found");
    }
    if (!context.objectPtrToId.contains(object)) {
        throw std::runtime_error("Object reference is stale");
    }
    return *object;
}

void initializeInstanceAttributes(const Module& module,
                                  ExecutionContext& context,
                                  FunctionType& functionType,
                                  ClassType& classType,
                                  NativeFunctionType& nativeFunctionType,
                                  ModuleType& moduleType,
                                  const HostRegistry& hosts,
                                  const std::shared_ptr<const Module>& modulePin,
                                  std::size_t classIndex,
                                  ScriptInstanceObject& instance);

Value makeScriptInstance(ExecutionContext& context,
                         FunctionType& functionType,
                         ClassType& classType,
                         NativeFunctionType& nativeFunctionType,
                         ModuleType& moduleType,
                         const HostRegistry& hosts,
                         ScriptInstanceType& instanceType,
                         const Module& module,
                         std::shared_ptr<const Module> modulePin,
                         std::size_t classIndex) {
    if (classIndex >= module.classes.size()) {
        throw std::runtime_error("Class index out of range");
    }

    const std::string& className = module.classes[classIndex].name;
    const auto instanceModulePin = modulePin;
    const Value instanceRef = emplaceObject(context,
                                            std::make_unique<ScriptInstanceObject>(instanceType,
                                                                                   classIndex,
                                                                                   className,
                                                                                   instanceModulePin));
    auto* instance = dynamic_cast<ScriptInstanceObject*>(&getObjectFromHeap(context, instanceRef));
    if (!instance) {
        throw std::runtime_error("Failed to create script instance");
    }
    initializeInstanceAttributes(module,
                                 context,
                                 functionType,
                                 classType,
                                 nativeFunctionType,
                                 moduleType,
                                 hosts,
                                 instanceModulePin,
                                 classIndex,
                                 *instance);
    return instanceRef;
}

bool tryFindClassMethodInModule(const Module& module,
                                std::size_t classIndex,
                                const std::string& methodName,
                                std::size_t& outFunctionIndex) {
    std::int32_t index = static_cast<std::int32_t>(classIndex);
    while (index >= 0) {
        const auto& cls = module.classes.at(static_cast<std::size_t>(index));
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

void initializeInstanceAttributes(const Module& module,
                                  ExecutionContext& context,
                                  FunctionType& functionType,
                                  ClassType& classType,
                                  NativeFunctionType& nativeFunctionType,
                                  ModuleType& moduleType,
                                  const HostRegistry& hosts,
                                  const std::shared_ptr<const Module>& modulePin,
                                  std::size_t classIndex,
                                  ScriptInstanceObject& instance) {
    if (classIndex >= module.classes.size()) {
        throw std::runtime_error("Class index out of range");
    }

    const auto& cls = module.classes[classIndex];
    if (cls.baseClassIndex >= 0) {
        initializeInstanceAttributes(module,
                                     context,
                                     functionType,
                                     classType,
                                     nativeFunctionType,
                                     moduleType,
                                     hosts,
                                     modulePin,
                                     static_cast<std::size_t>(cls.baseClassIndex),
                                     instance);
    }

    for (const auto& attr : cls.attributes) {
        instance.fields()[attr.name] = normalizeRuntimeValue(context,
                                                             functionType,
                                                             classType,
                                                             nativeFunctionType,
                                                             moduleType,
                                                             hosts,
                                                             modulePin,
                                                             attr.defaultValue,
                                                             true);
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

void VirtualMachine::pushCallFrame(ExecutionContext& ctx,
                                   std::shared_ptr<const Module> modulePin,
                                   std::size_t functionIndex,
                                   const std::vector<Value>& args,
                                   bool replaceReturnWithInstance,
                                   Value constructorInstance) {
    if (!modulePin) {
        throw std::runtime_error("Call frame module is null");
    }

    const auto& fn = modulePin->functions.at(functionIndex);
    if (args.size() != fn.params.size()) {
        throw std::runtime_error("Function argument count mismatch: " + fn.name);
    }

    Frame frame;
    frame.functionIndex = functionIndex;
    frame.ip = 0;
    frame.modulePin = std::move(modulePin);
    frame.replaceReturnWithInstance = replaceReturnWithInstance;
    frame.constructorInstance = constructorInstance;
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
    Object* object = ref.asRef();
    if (!object) {
        throw std::runtime_error("Object reference not found");
    }
    if (!context.objectPtrToId.contains(object)) {
        throw std::runtime_error("Object reference is stale");
    }
    return *object;
}

bool VirtualMachine::execute(ExecutionContext& context, std::size_t stepBudget) {
    std::size_t steps = 0;
    while (steps++ < stepBudget) {
        if (context.frames.empty()) {
            return true;
        }

        auto& frame = context.frames.back();
        if (!frame.modulePin) {
            throw std::runtime_error("Frame module is null");
        }
        const auto frameModule = frame.modulePin;
        const auto& fn = frameModule->functions.at(frame.functionIndex);
        context.modulePin = frameModule;
        if (frame.ip >= fn.code.size()) {
            throw std::runtime_error("Instruction pointer out of range");
        }

        const Instruction ins = fn.code[frame.ip++];
        switch (ins.op) {
        case OpCode::PushConst: {
            Value value = normalizeRuntimeValue(context,
                                                functionType_,
                                                classType_,
                                                nativeFunctionType_,
                                                moduleType_,
                                                hosts_,
                                                frameModule,
                                                frameModule->constants.at(ins.a),
                                                true);
            frame.stack.push_back(value);
            break;
        }
        case OpCode::LoadName: {
            const auto& symbolName = frameModule->strings.at(ins.a);
            frame.stack.push_back(resolveRuntimeName(context,
                                                     functionType_,
                                                     classType_,
                                                     nativeFunctionType_,
                                                     moduleType_,
                                                     hosts_,
                                                     frameModule,
                                                     symbolName));
            break;
        }
        case OpCode::LoadLocal:
            frame.stack.push_back(normalizeRuntimeValue(context,
                                                        functionType_,
                                                        classType_,
                                                        nativeFunctionType_,
                                                        moduleType_,
                                                        hosts_,
                                                        frameModule,
                                                        frame.locals.at(ins.a),
                                                        false));
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
            const auto& name = frameModule->strings.at(ins.a);
            VmHostContext hostContext(context);
            frame.stack.push_back(hosts_.invoke(name, hostContext, args));
            break;
        }
        case OpCode::CallFunc: {
            const auto args = collectArgs(frame.stack, static_cast<std::size_t>(ins.b));
            pushCallFrame(context, frameModule, static_cast<std::size_t>(ins.a), args);
            break;
        }
        case OpCode::NewInstance: {
            const auto ctorArgs = collectArgs(frame.stack, static_cast<std::size_t>(ins.b));
            const std::size_t classIndex = static_cast<std::size_t>(ins.a);
            const Value instanceRef = makeScriptInstance(context,
                                                         functionType_,
                                                         classType_,
                                                         nativeFunctionType_,
                                                         moduleType_,
                                                         hosts_,
                                                         instanceType_,
                                                         *frameModule,
                                                         frameModule,
                                                         classIndex);

            std::size_t ctorFunctionIndex = 0;
            if (!tryFindClassMethodInModule(*frameModule, classIndex, "__new__", ctorFunctionIndex)) {
                throw std::runtime_error("Class is missing required constructor __new__: " + frameModule->classes.at(classIndex).name);
            }

            std::vector<Value> ctorInvokeArgs;
            ctorInvokeArgs.reserve(ctorArgs.size() + 1);
            ctorInvokeArgs.push_back(instanceRef);
            ctorInvokeArgs.insert(ctorInvokeArgs.end(), ctorArgs.begin(), ctorArgs.end());
            pushCallFrame(context,
                          frameModule,
                          ctorFunctionIndex,
                          ctorInvokeArgs,
                          true,
                          instanceRef);
            break;
        }
        case OpCode::LoadAttr: {
            const Value selfRef = popValue(frame.stack);
            Object& object = getObject(context, selfRef);
            const auto& attrName = frameModule->strings.at(ins.a);
            if (auto* instance = dynamic_cast<ScriptInstanceObject*>(&object)) {
                auto it = instance->fields().find(attrName);
                if (it == instance->fields().end()) {
                    throw std::runtime_error("Unknown class attribute: " + attrName);
                }
                const auto valueModule = instance->modulePin() ? instance->modulePin() : frameModule;
                it->second = normalizeRuntimeValue(context,
                                                   functionType_,
                                                   classType_,
                                                   nativeFunctionType_,
                                                   moduleType_,
                                                   hosts_,
                                                   valueModule,
                                                   it->second,
                                                   false);
                frame.stack.push_back(it->second);
            } else if (auto* moduleObj = dynamic_cast<ModuleObject*>(&object)) {
                auto exportIt = moduleObj->exports().find(attrName);
                if (exportIt != moduleObj->exports().end()) {
                    frame.stack.push_back(exportIt->second);
                    break;
                }

                const auto& modulePin = moduleObj->modulePin();
                if (modulePin) {
                    for (const auto& global : modulePin->globals) {
                        if (global.name == attrName) {
                            Value globalValue = normalizeRuntimeValue(context,
                                                                      functionType_,
                                                                      classType_,
                                                                      nativeFunctionType_,
                                                                      moduleType_,
                                                                      hosts_,
                                                                      modulePin,
                                                                      global.initialValue,
                                                                      true);
                            rememberWriteBarrier(context, *moduleObj, globalValue);
                            moduleObj->exports()[attrName] = globalValue;
                            frame.stack.push_back(globalValue);
                            goto load_attr_done;
                        }
                    }

                    for (std::size_t i = 0; i < modulePin->functions.size(); ++i) {
                        const auto& fn = modulePin->functions[i];
                        if (fn.name == attrName) {
                            Value functionRef = makeFunctionObject(context, functionType_, i, modulePin);
                            rememberWriteBarrier(context, *moduleObj, functionRef);
                            moduleObj->exports()[attrName] = functionRef;
                            frame.stack.push_back(functionRef);
                            goto load_attr_done;
                        }
                    }

                    for (std::size_t i = 0; i < modulePin->classes.size(); ++i) {
                        const auto& cls = modulePin->classes[i];
                        if (cls.name == attrName) {
                            Value classRef = emplaceObject(context,
                                                           std::make_unique<ClassObject>(classType_,
                                                                                         cls.name,
                                                                                         i,
                                                                                         modulePin));
                            rememberWriteBarrier(context, *moduleObj, classRef);
                            moduleObj->exports()[attrName] = classRef;
                            frame.stack.push_back(classRef);
                            goto load_attr_done;
                        }
                    }
                }

                frame.stack.push_back(object.getType().getMember(object, attrName));
            } else {
                frame.stack.push_back(object.getType().getMember(object, attrName));
            }
load_attr_done:
            break;
        }
        case OpCode::StoreAttr: {
            const Value assigned = popValue(frame.stack);
            const Value selfRef = popValue(frame.stack);
            Object& object = getObject(context, selfRef);
            const auto& attrName = frameModule->strings.at(ins.a);
            const Value normalized = normalizeRuntimeValue(context,
                                                           functionType_,
                                                           classType_,
                                                           nativeFunctionType_,
                                                           moduleType_,
                                                           hosts_,
                                                           frameModule,
                                                           assigned,
                                                           false);
            if (auto* instance = dynamic_cast<ScriptInstanceObject*>(&object)) {
                rememberWriteBarrier(context, *instance, normalized);
                instance->fields()[attrName] = normalized;
                frame.stack.push_back(normalized);
            } else {
                rememberWriteBarrier(context, object, normalized);
                frame.stack.push_back(object.getType().setMember(object, attrName, normalized));
            }
            break;
        }
        case OpCode::CallMethod: {
            const auto args = collectArgs(frame.stack, static_cast<std::size_t>(ins.b));
            const Value selfRef = popValue(frame.stack);
            Object& object = getObject(context, selfRef);
            const auto& methodName = frameModule->strings.at(ins.a);

            const auto makeString = [&](const std::string& text) {
                return makeRuntimeString(context, text);
            };
            const auto valueStr = [&](const Value& nested) {
                return __str__Value(context, nested);
            };

            if (auto* moduleObj = dynamic_cast<ModuleObject*>(&object)) {
                const auto& modulePin = moduleObj->modulePin();
                if (!modulePin) {
                    throw std::runtime_error("Module object is not loaded: " + moduleObj->moduleName());
                }

                auto exportIt = moduleObj->exports().find(methodName);
                if (exportIt != moduleObj->exports().end()) {
                    const Value callable = normalizeRuntimeValue(context,
                                                                 functionType_,
                                                                 classType_,
                                                                 nativeFunctionType_,
                                                                 moduleType_,
                                                                 hosts_,
                                                                 modulePin,
                                                                 exportIt->second,
                                                                 false);
                    if (!valueEquals(context, exportIt->second, callable)) {
                        rememberWriteBarrier(context, *moduleObj, callable);
                        exportIt->second = callable;
                    }

                    if (!callable.isRef()) {
                        throw std::runtime_error("Module export is not callable: " + methodName);
                    }

                    Object& callableObject = getObject(context, callable);
                    if (auto* fnObject = dynamic_cast<FunctionObject*>(&callableObject)) {
                        const auto callModule = fnObject->modulePin() ? fnObject->modulePin() : modulePin;
                        pushCallFrame(context,
                                      callModule,
                                      fnObject->functionIndex(),
                                      args);
                        goto call_method_done;
                    }

                    auto* classObject = dynamic_cast<ClassObject*>(&callableObject);
                    if (classObject) {
                        const auto& targetModule = classObject->modulePin();
                        if (!targetModule) {
                            throw std::runtime_error("Class object is not bound to module: " + classObject->className());
                        }

                        const std::size_t classIndex = classObject->classIndex();
                        const Value instanceRef = makeScriptInstance(context,
                                                                     functionType_,
                                                                     classType_,
                                                                     nativeFunctionType_,
                                                                     moduleType_,
                                                                     hosts_,
                                                                     instanceType_,
                                                                     *targetModule,
                                                                     targetModule,
                                                                     classIndex);

                        std::size_t ctorFunctionIndex = 0;
                        if (!tryFindClassMethodInModule(*targetModule, classIndex, "__new__", ctorFunctionIndex)) {
                            throw std::runtime_error("Class is missing required constructor __new__: " + classObject->className());
                        }

                        std::vector<Value> ctorInvokeArgs;
                        ctorInvokeArgs.reserve(args.size() + 1);
                        ctorInvokeArgs.push_back(instanceRef);
                        ctorInvokeArgs.insert(ctorInvokeArgs.end(), args.begin(), args.end());
                        pushCallFrame(context,
                                      targetModule,
                                      ctorFunctionIndex,
                                      ctorInvokeArgs,
                                      true,
                                      instanceRef);
                        goto call_method_done;
                    }

                    if (auto* nativeFunction = dynamic_cast<NativeFunctionObject*>(&callableObject)) {
                        VmHostContext hostContext(context);
                        frame.stack.push_back(nativeFunction->invoke(hostContext, args));
                        goto call_method_done;
                    }

                    throw std::runtime_error("Module export is not function or class: " + methodName);
                }

                for (std::size_t i = 0; i < modulePin->functions.size(); ++i) {
                    if (modulePin->functions[i].name == methodName) {
                        pushCallFrame(context,
                                      modulePin,
                                      i,
                                      args);
                        goto call_method_done;
                    }
                }

                for (std::size_t i = 0; i < modulePin->classes.size(); ++i) {
                    if (modulePin->classes[i].name != methodName) {
                        continue;
                    }

                    const Value instanceRef = makeScriptInstance(context,
                                                                 functionType_,
                                                                 classType_,
                                                                 nativeFunctionType_,
                                                                 moduleType_,
                                                                 hosts_,
                                                                 instanceType_,
                                                                 *modulePin,
                                                                 modulePin,
                                                                 i);

                    std::size_t ctorFunctionIndex = 0;
                    if (!tryFindClassMethodInModule(*modulePin, i, "__new__", ctorFunctionIndex)) {
                        throw std::runtime_error("Class is missing required constructor __new__: " + methodName);
                    }

                    std::vector<Value> ctorInvokeArgs;
                    ctorInvokeArgs.reserve(args.size() + 1);
                    ctorInvokeArgs.push_back(instanceRef);
                    ctorInvokeArgs.insert(ctorInvokeArgs.end(), args.begin(), args.end());
                    pushCallFrame(context,
                                  modulePin,
                                  ctorFunctionIndex,
                                  ctorInvokeArgs,
                                  true,
                                  instanceRef);
                    goto call_method_done;
                }

                throw std::runtime_error("Script function not found: " + methodName);
            }

            if (auto* list = dynamic_cast<ListObject*>(&object)) {
                if (methodName == "push" && !args.empty()) {
                    rememberWriteBarrier(context, *list, args[0]);
                } else if (methodName == "set" && args.size() >= 2) {
                    rememberWriteBarrier(context, *list, args[1]);
                }
            } else if (auto* dict = dynamic_cast<DictObject*>(&object)) {
                if (methodName == "set" && args.size() >= 2) {
                    rememberWriteBarrier(context, *dict, args[1]);
                }
            }

            if (auto* instance = dynamic_cast<ScriptInstanceObject*>(&object)) {
                auto fieldIt = instance->fields().find(methodName);
                if (fieldIt != instance->fields().end()) {
                    const auto callValueModule = instance->modulePin() ? instance->modulePin() : frameModule;
                    const Value callable = normalizeRuntimeValue(context,
                                                                 functionType_,
                                                                 classType_,
                                                                 nativeFunctionType_,
                                                                 moduleType_,
                                                                 hosts_,
                                                                 callValueModule,
                                                                 fieldIt->second,
                                                                 false);
                    rememberWriteBarrier(context, *instance, callable);
                    fieldIt->second = callable;
                    if (!callable.isRef()) {
                        throw std::runtime_error("Object property is not callable: " + methodName);
                    }

                    Object& callableObject = getObject(context, callable);
                    auto* fnObject = dynamic_cast<FunctionObject*>(&callableObject);
                    if (!fnObject) {
                        throw std::runtime_error("Object property is not function object: " + methodName);
                    }

                    const auto callModule = fnObject->modulePin() ? fnObject->modulePin() : frameModule;
                    pushCallFrame(context,
                                  callModule,
                                  fnObject->functionIndex(),
                                  args);
                    break;
                }

                std::size_t classMethodIndex = 0;
                auto methodModule = instance->modulePin() ? instance->modulePin() : frameModule;
                if (tryFindClassMethodInModule(*methodModule, instance->classIndex(), methodName, classMethodIndex)) {
                    std::vector<Value> methodArgs;
                    methodArgs.reserve(args.size() + 1);
                    methodArgs.push_back(selfRef);
                    methodArgs.insert(methodArgs.end(), args.begin(), args.end());
                    pushCallFrame(context,
                                  methodModule,
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
call_method_done:
            break;
        }
        case OpCode::CallValue: {
            const auto args = collectArgs(frame.stack, static_cast<std::size_t>(ins.a));
            Value callable = popValue(frame.stack);
            callable = normalizeRuntimeValue(context,
                                             functionType_,
                                             classType_,
                                             nativeFunctionType_,
                                             moduleType_,
                                             hosts_,
                                             frameModule,
                                             callable,
                                             false);
            if (!callable.isRef()) {
                throw std::runtime_error("Attempted to call a non-function value");
            }

            Object& callableObject = getObject(context, callable);
            auto* fnObject = dynamic_cast<FunctionObject*>(&callableObject);
            if (fnObject) {
                const auto callModule = fnObject->modulePin() ? fnObject->modulePin() : frameModule;
                pushCallFrame(context,
                              callModule,
                              fnObject->functionIndex(),
                              args);
                break;
            }

            auto* classObject = dynamic_cast<ClassObject*>(&callableObject);
            if (classObject) {
                const auto& targetModule = classObject->modulePin();
                if (!targetModule) {
                    throw std::runtime_error("Class object is not bound to module: " + classObject->className());
                }

                const std::size_t classIndex = classObject->classIndex();
                const Value instanceRef = makeScriptInstance(context,
                                                             functionType_,
                                                             classType_,
                                                             nativeFunctionType_,
                                                             moduleType_,
                                                             hosts_,
                                                             instanceType_,
                                                             *targetModule,
                                                             targetModule,
                                                             classIndex);

                std::size_t ctorFunctionIndex = 0;
                if (!tryFindClassMethodInModule(*targetModule, classIndex, "__new__", ctorFunctionIndex)) {
                    throw std::runtime_error("Class is missing required constructor __new__: " + classObject->className());
                }

                std::vector<Value> ctorInvokeArgs;
                ctorInvokeArgs.reserve(args.size() + 1);
                ctorInvokeArgs.push_back(instanceRef);
                ctorInvokeArgs.insert(ctorInvokeArgs.end(), args.begin(), args.end());
                pushCallFrame(context,
                              targetModule,
                              ctorFunctionIndex,
                              ctorInvokeArgs,
                              true,
                              instanceRef);
                break;
            }

            if (auto* nativeFunction = dynamic_cast<NativeFunctionObject*>(&callableObject)) {
                VmHostContext hostContext(context);
                frame.stack.push_back(nativeFunction->invoke(hostContext, args));
                break;
            }

            throw std::runtime_error("Attempted to call a non-function object");
        }
        case OpCode::CallIntrinsic:
            throw std::runtime_error("CallIntrinsic is deprecated. Use Type exported methods.");
        case OpCode::SpawnFunc: {
            const auto args = collectArgs(frame.stack, static_cast<std::size_t>(ins.b));
            const auto funcName = frameModule->functions.at(ins.a).name;
            const auto module = frameModule;
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
            std::this_thread::sleep_for(std::chrono::milliseconds(ins.a));
            break;
        case OpCode::Yield:
            std::this_thread::yield();
            break;
        case OpCode::Return: {
            Value ret = frame.stack.empty() ? Value::Nil() : popValue(frame.stack);
            if (frame.replaceReturnWithInstance) {
                ret = frame.constructorInstance;
            }
            context.frames.pop_back();
            if (context.frames.empty()) {
                context.returnValue = ret;
                return true;
            }
            context.frames.back().stack.push_back(ret);
            break;
        }
        case OpCode::Pop:
            (void)popValue(frame.stack);
            break;
        }

        runGcSlice(context, context.gc.sliceBudgetObjects);
    }

    return context.frames.empty();
}

Value VirtualMachine::runFunction(const std::string& functionName, const std::vector<Value>& args) {
    ExecutionContext ctx;
    ctx.modulePin = module_;
    ctx.stringPool = module_->strings;
    pushCallFrame(ctx, module_, findFunctionIndex(functionName), args);

    while (!execute(ctx, 1000)) {
    }

    runDeleteHooks(ctx);
    return ctx.returnValue;
}

void VirtualMachine::runDeleteHooks(ExecutionContext& context) {
    if (context.deleteHooksRan) {
        return;
    }
    context.deleteHooksRan = true;

    struct DeleteTask {
        Value objectRef{Value::Nil()};
        std::size_t classIndex{0};
        std::shared_ptr<const Module> modulePin;
    };

    std::vector<DeleteTask> tasks;
    tasks.reserve(context.objectHeap.size());
    for (const auto& [objectId, objectPtr] : context.objectHeap) {
        if (!objectPtr) {
            continue;
        }
        auto* instance = dynamic_cast<ScriptInstanceObject*>(objectPtr.get());
        if (!instance) {
            continue;
        }
        auto modulePin = instance->modulePin() ? instance->modulePin() : module_;
        if (!modulePin) {
            continue;
        }
        tasks.push_back({Value::Ref(objectPtr.get()), instance->classIndex(), modulePin});
    }

    for (const auto& task : tasks) {
        if (!task.objectRef.isRef() || task.objectRef.asRef() == nullptr) {
            continue;
        }

        std::size_t deleteFunctionIndex = 0;
        if (!tryFindClassMethodInModule(*task.modulePin,
                                        task.classIndex,
                                        "__delete__",
                                        deleteFunctionIndex)) {
            continue;
        }

        pushCallFrame(context,
                      task.modulePin,
                      deleteFunctionIndex,
                      {task.objectRef});

        while (!execute(context, 1000)) {
        }
    }
}

} // namespace gs
