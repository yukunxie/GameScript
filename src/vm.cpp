#include "gs/vm.hpp"

#include <chrono>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace gs {

namespace {

void pushRaw(std::vector<Value>& stack, std::size_t& stackTop, const Value& value) {
    if (stackTop >= stack.size()) {
        const std::size_t nextSize = stack.empty() ? 8 : (stack.size() * 2);
        stack.resize(nextSize, Value::Nil());
    }
    stack[stackTop++] = value;
}

Value popRaw(std::vector<Value>& stack, std::size_t& stackTop) {
    if (stackTop == 0) {
        throw std::runtime_error("Stack underflow");
    }
    --stackTop;
    return stack[stackTop];
}

void collectArgs(std::vector<Value>& stack,
                 std::size_t& stackTop,
                 std::size_t count,
                 std::vector<Value>& outArgs) {
    if (stackTop < count) {
        throw std::runtime_error("Not enough arguments on stack");
    }
    outArgs.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        outArgs[count - 1 - i] = popRaw(stack, stackTop);
    }
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

    if (auto* lambdaObject = dynamic_cast<LambdaObject*>(object)) {
        for (const auto& value : lambdaObject->captures()) {
            markChild(value);
        }
        return;
    }

    if (auto* cell = dynamic_cast<UpvalueCellObject*>(object)) {
        markChild(cell->value());
        return;
    }
}

void markRoots(ExecutionContext& context, bool youngOnly) {
    for (const auto& frame : context.frames) {
        markValue(context, frame.constructorInstance, youngOnly);
        markValue(context, frame.registerValue, youngOnly);
        for (const auto& regValue : frame.registers) {
            markValue(context, regValue, youngOnly);
        }
        for (const auto& value : frame.locals) {
            markValue(context, value, youngOnly);
        }
        for (const auto& capture : frame.captures) {
            markValue(context, capture, youngOnly);
        }
        for (std::size_t i = 0; i < frame.stackTop; ++i) {
            markValue(context, frame.stack[i], youngOnly);
        }
    }
    markValue(context, context.returnValue, youngOnly);

    for (const auto& [modulePtr, globals] : context.moduleRuntimeGlobals) {
        (void)modulePtr;
        for (const auto& [name, value] : globals) {
            (void)name;
            markValue(context, value, youngOnly);
        }
    }

    for (const auto& [modulePtr, moduleRef] : context.moduleRuntimeObjects) {
        (void)modulePtr;
        markValue(context, moduleRef, youngOnly);
    }

    for (const auto& [cacheKey, moduleRef] : context.moduleObjectCache) {
        (void)cacheKey;
        markValue(context, moduleRef, youngOnly);
    }

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
    case ValueType::Float: {
        std::ostringstream out;
        out << std::setprecision(17) << value.asFloat();
        return out.str();
    }
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

bool isNumericValue(const Value& value) {
    return value.isInt() || value.isFloat();
}

double toDouble(const Value& value) {
    if (value.isFloat()) {
        return value.asFloat();
    }
    if (value.isInt()) {
        return static_cast<double>(value.asInt());
    }
    throw std::runtime_error("Value is not numeric");
}

std::int64_t toBoolInt(const Value& value) {
    if (value.isNil()) {
        return 0;
    }
    if (value.isInt()) {
        return value.asInt() != 0 ? 1 : 0;
    }
    if (value.isFloat()) {
        return std::abs(value.asFloat()) > std::numeric_limits<double>::epsilon() ? 1 : 0;
    }
    return 1;
}

bool isTruthy(const ExecutionContext& context, const Value& value) {
    return toBoolInt(value) != 0;
}

std::string typeNameOfValue(const ExecutionContext& context, const Value& value) {
    switch (value.type) {
    case ValueType::Nil:
        return "nil";
    case ValueType::Int:
        return "int";
    case ValueType::Float:
        return "float";
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
    if (isNumericValue(lhs) && isNumericValue(rhs)) {
        return std::abs(toDouble(lhs) - toDouble(rhs)) <= std::numeric_limits<double>::epsilon();
    }
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
    explicit VmHostContext(ExecutionContext& context) : vm_(nullptr), context_(context) {}
    VmHostContext(VirtualMachine& vm, ExecutionContext& context) : vm_(&vm), context_(context) {}

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

    void ensureModuleInitialized(const Value& moduleRef) override {
        if (!moduleRef.isRef()) {
            throw std::runtime_error("loadModule result is not an object reference");
        }

        Object& object = getObject(moduleRef);
        auto* moduleObject = dynamic_cast<ModuleObject*>(&object);
        if (!moduleObject) {
            throw std::runtime_error("loadModule result is not a module object");
        }

        const auto modulePin = moduleObject->modulePin();
        if (!modulePin) {
            return;
        }

        context_.moduleRuntimeObjects[modulePin.get()] = moduleRef;
        if (!vm_) {
            throw std::runtime_error("Module initialization is unavailable in this host context");
        }
        vm_->ensureModuleInitialized(context_, modulePin);
    }

    bool tryGetCachedModuleObject(const std::string& moduleKey, Value& outModuleRef) override {
        auto it = context_.moduleObjectCache.find(moduleKey);
        if (it == context_.moduleObjectCache.end()) {
            return false;
        }
        outModuleRef = it->second;
        return true;
    }

    void cacheModuleObject(const std::string& moduleKey, const Value& moduleRef) override {
        context_.moduleObjectCache[moduleKey] = moduleRef;
        if (moduleRef.isRef()) {
            Object& object = getObject(moduleRef);
            if (auto* moduleObject = dynamic_cast<ModuleObject*>(&object)) {
                if (moduleObject->modulePin()) {
                    context_.moduleRuntimeObjects[moduleObject->modulePin().get()] = moduleRef;
                }
            }
        }
    }

private:
    VirtualMachine* vm_;
    ExecutionContext& context_;
};

Value makeFunctionObject(ExecutionContext& context,
                         FunctionType& functionType,
                         std::size_t functionIndex,
                         std::shared_ptr<const Module> modulePin = nullptr) {
    return emplaceObject(context, std::make_unique<FunctionObject>(functionType, functionIndex, std::move(modulePin)));
}

Value makeLambdaObject(ExecutionContext& context,
                       LambdaType& lambdaType,
                       std::size_t functionIndex,
                       std::shared_ptr<const Module> modulePin,
                       std::vector<Value> captures) {
    return emplaceObject(context,
                         std::make_unique<LambdaObject>(lambdaType,
                                                        functionIndex,
                                                        std::move(modulePin),
                                                        std::move(captures)));
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

    auto moduleGlobalsIt = context.moduleRuntimeGlobals.find(frameModule.get());
    if (moduleGlobalsIt != context.moduleRuntimeGlobals.end()) {
        auto nameIt = moduleGlobalsIt->second.find(name);
        if (nameIt != moduleGlobalsIt->second.end()) {
            return nameIt->second;
        }
    }

    for (const auto& global : frameModule->globals) {
        if (global.name == name) {
            Value normalized = normalizeRuntimeValue(context,
                                                     functionType,
                                                     classType,
                                                     nativeFunctionType,
                                                     moduleType,
                                                     hosts,
                                                     frameModule,
                                                     global.initialValue,
                                                     true);
            context.moduleRuntimeGlobals[frameModule.get()][name] = normalized;
            return normalized;
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
        VmHostContext hostContext(context);
        return hosts.resolveBuiltin(name,
                                    hostContext,
                                    nativeFunctionType,
                                    moduleType);
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

void storeRuntimeGlobal(ExecutionContext& context,
                        const std::shared_ptr<const Module>& frameModule,
                        const std::string& symbolName,
                        const Value& value) {
    context.moduleRuntimeGlobals[frameModule.get()][symbolName] = value;

    auto moduleRefIt = context.moduleRuntimeObjects.find(frameModule.get());
    if (moduleRefIt != context.moduleRuntimeObjects.end() && moduleRefIt->second.isRef()) {
        Object& moduleObjectBase = getObjectFromHeap(context, moduleRefIt->second);
        if (auto* moduleObject = dynamic_cast<ModuleObject*>(&moduleObjectBase)) {
            rememberWriteBarrier(context, *moduleObject, value);
            moduleObject->exports()[symbolName] = value;
        }
    }
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

void VirtualMachine::ensureModuleInitialized(ExecutionContext& context, const std::shared_ptr<const Module>& modulePin) {
    if (!modulePin) {
        return;
    }

    const Module* moduleKey = modulePin.get();
    if (context.initializedModules.contains(moduleKey)) {
        return;
    }
    if (context.moduleInitInProgress.contains(moduleKey)) {
        return;
    }

    std::size_t moduleInitIndex = static_cast<std::size_t>(-1);
    for (std::size_t i = 0; i < modulePin->functions.size(); ++i) {
        if (modulePin->functions[i].name == "__module_init__") {
            moduleInitIndex = i;
            break;
        }
    }

    context.moduleInitInProgress.insert(moduleKey);
    try {
        if (moduleInitIndex != static_cast<std::size_t>(-1)) {
            const std::size_t baseFrameCount = context.frames.size();
            pushCallFrame(context, modulePin, moduleInitIndex, {});
            std::size_t guard = 0;
            while (context.frames.size() > baseFrameCount) {
                if (++guard > 1000000) {
                    throw std::runtime_error("Module initialization did not converge");
                }
                (void)execute(context, 1);
            }
        }
        context.initializedModules.insert(moduleKey);
        context.moduleInitInProgress.erase(moduleKey);
    } catch (...) {
        context.moduleInitInProgress.erase(moduleKey);
        throw;
    }
}

void VirtualMachine::pushCallFrame(ExecutionContext& ctx,
                                   std::shared_ptr<const Module> modulePin,
                                   std::size_t functionIndex,
                                   const std::vector<Value>& args,
                                   bool replaceReturnWithInstance,
                                   Value constructorInstance,
                                   std::vector<Value> captures) {
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
    frame.captures = std::move(captures);
    frame.stack.assign(fn.stackSlotCount, Value::Nil());
    frame.stackTop = 0;
    frame.registerValue = Value::Nil();
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
    std::vector<Value> argScratch;
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
        const auto readRegister = [&](std::int32_t index) -> Value {
            if (index < 0 || index >= static_cast<std::int32_t>(frame.registers.size())) {
                throw std::runtime_error("Register index out of range");
            }
            if (index == 0) {
                return frame.registerValue;
            }
            return frame.registers[static_cast<std::size_t>(index)];
        };

        const auto writeRegister = [&](std::int32_t index, const Value& value) {
            if (index < 0 || index >= static_cast<std::int32_t>(frame.registers.size())) {
                throw std::runtime_error("Register index out of range");
            }
            frame.registers[static_cast<std::size_t>(index)] = value;
            if (index == 0) {
                frame.registerValue = value;
            }
        };

        const auto resolveSlotValue = [&](SlotType slotType, std::int32_t index) -> Value {
            switch (slotType) {
            case SlotType::None:
                return Value::Nil();
            case SlotType::Local:
                return normalizeRuntimeValue(context,
                                             functionType_,
                                             classType_,
                                             nativeFunctionType_,
                                             moduleType_,
                                             hosts_,
                                             frameModule,
                                             [&]() -> Value {
                                                 const Value& localValue = frame.locals.at(index);
                                                 if (localValue.isRef()) {
                                                     Object* localObject = localValue.asRef();
                                                     if (localObject && dynamic_cast<UpvalueCellObject*>(localObject)) {
                                                         return dynamic_cast<UpvalueCellObject*>(localObject)->value();
                                                     }
                                                 }
                                                 return localValue;
                                             }(),
                                             false);
            case SlotType::Constant:
                return normalizeRuntimeValue(context,
                                             functionType_,
                                             classType_,
                                             nativeFunctionType_,
                                             moduleType_,
                                             hosts_,
                                             frameModule,
                                             frameModule->constants.at(index),
                                             true);
            case SlotType::Register:
                return normalizeRuntimeValue(context,
                                             functionType_,
                                             classType_,
                                             nativeFunctionType_,
                                             moduleType_,
                                             hosts_,
                                             frameModule,
                                             readRegister(index),
                                             false);
            case SlotType::UpValue: {
                if (index < 0 || static_cast<std::size_t>(index) >= frame.captures.size()) {
                    throw std::runtime_error("Capture index out of range");
                }
                const Value captureRef = frame.captures[static_cast<std::size_t>(index)];
                Object& object = getObject(context, captureRef);
                auto* cell = dynamic_cast<UpvalueCellObject*>(&object);
                if (!cell) {
                    throw std::runtime_error("Capture is not an upvalue cell");
                }
                return normalizeRuntimeValue(context,
                                             functionType_,
                                             classType_,
                                             nativeFunctionType_,
                                             moduleType_,
                                             hosts_,
                                             frameModule,
                                             cell->value(),
                                             false);
            }
            }
            return Value::Nil();
        };

        const auto tryInvokeScriptCallable = [&](Object& callableObject,
                                                 const std::shared_ptr<const Module>& fallbackModule,
                                                 const std::vector<Value>& invokeArgs,
                                                 const std::string& missingBindingMessage) -> bool {
            if (auto* lambdaObject = dynamic_cast<LambdaObject*>(&callableObject)) {
                const auto callModule = lambdaObject->modulePin() ? lambdaObject->modulePin() : fallbackModule;
                if (!callModule) {
                    throw std::runtime_error(missingBindingMessage);
                }
                pushCallFrame(context,
                              callModule,
                              lambdaObject->functionIndex(),
                              invokeArgs,
                              false,
                              Value::Nil(),
                              lambdaObject->captures());
                return true;
            }

            if (auto* fnObject = dynamic_cast<FunctionObject*>(&callableObject)) {
                const auto callModule = fnObject->modulePin() ? fnObject->modulePin() : fallbackModule;
                if (!callModule) {
                    throw std::runtime_error(missingBindingMessage);
                }
                pushCallFrame(context,
                              callModule,
                              fnObject->functionIndex(),
                              invokeArgs);
                return true;
            }

            return false;
        };

        const auto tryInvokeClassOrNativeCallable = [&](Object& callableObject,
                                                        const std::vector<Value>& invokeArgs) -> bool {
            if (auto* classObject = dynamic_cast<ClassObject*>(&callableObject)) {
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
                ctorInvokeArgs.reserve(invokeArgs.size() + 1);
                ctorInvokeArgs.push_back(instanceRef);
                ctorInvokeArgs.insert(ctorInvokeArgs.end(), invokeArgs.begin(), invokeArgs.end());
                pushCallFrame(context,
                              targetModule,
                              ctorFunctionIndex,
                              ctorInvokeArgs,
                              true,
                              instanceRef);
                return true;
            }

            if (auto* nativeFunction = dynamic_cast<NativeFunctionObject*>(&callableObject)) {
                const std::size_t callerFrameIndex = context.frames.size() - 1;
                VmHostContext hostContext(*this, context);
                const Value result = nativeFunction->invoke(hostContext, invokeArgs);
                if (callerFrameIndex < context.frames.size()) {
                    pushRaw(context.frames[callerFrameIndex].stack,
                            context.frames[callerFrameIndex].stackTop,
                            result);
                }
                return true;
            }

            return false;
        };

        const auto tryInvokeModuleNamedCallable = [&](const std::shared_ptr<const Module>& targetModule,
                                                      const std::string& callableName,
                                                      const std::vector<Value>& invokeArgs) -> bool {
            if (!targetModule) {
                return false;
            }

            for (std::size_t i = 0; i < targetModule->functions.size(); ++i) {
                if (targetModule->functions[i].name == callableName) {
                    pushCallFrame(context,
                                  targetModule,
                                  i,
                                  invokeArgs);
                    return true;
                }
            }

            for (std::size_t i = 0; i < targetModule->classes.size(); ++i) {
                if (targetModule->classes[i].name != callableName) {
                    continue;
                }

                const Value classRef = makeClassObjectValue(context,
                                                            classType_,
                                                            targetModule,
                                                            i);
                Object& classObject = getObject(context, classRef);
                return tryInvokeClassOrNativeCallable(classObject, invokeArgs);
            }

            return false;
        };

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
            pushRaw(frame.stack, frame.stackTop, value);
            break;
        }
        case OpCode::LoadName: {
            const auto& symbolName = frameModule->strings.at(ins.a);
            pushRaw(frame.stack, frame.stackTop, resolveRuntimeName(context,
                                                     functionType_,
                                                     classType_,
                                                     nativeFunctionType_,
                                                     moduleType_,
                                                     hosts_,
                                                     frameModule,
                                                     symbolName));
            break;
        }
        case OpCode::PushName: {
            const auto& symbolName = frameModule->strings.at(ins.a);
            pushRaw(frame.stack, frame.stackTop, resolveRuntimeName(context,
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
            pushRaw(frame.stack, frame.stackTop, resolveSlotValue(SlotType::Local, ins.a));
            break;
        case OpCode::PushLocal:
            pushRaw(frame.stack, frame.stackTop, resolveSlotValue(SlotType::Local, ins.a));
            break;
        case OpCode::StoreLocal: {
            const Value v = popRaw(frame.stack, frame.stackTop);
            Value& localValue = frame.locals.at(ins.a);
            if (localValue.isRef()) {
                Object* localObject = localValue.asRef();
                if (localObject) {
                    if (auto* cell = dynamic_cast<UpvalueCellObject*>(localObject)) {
                        rememberWriteBarrier(context, *cell, v);
                        cell->value() = v;
                        break;
                    }
                }
            }
            localValue = v;
            break;
        }
        case OpCode::StoreName: {
            const Value value = popRaw(frame.stack, frame.stackTop);
            const auto& symbolName = frameModule->strings.at(ins.a);
            storeRuntimeGlobal(context, frameModule, symbolName, value);
            break;
        }
        case OpCode::Add: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                Value out = Value::Nil();
                if (lhs.isInt() && rhs.isInt()) {
                    out = Value::Int(lhs.asInt() + rhs.asInt());
                } else if (isNumericValue(lhs) && isNumericValue(rhs)) {
                    out = Value::Float(toDouble(lhs) + toDouble(rhs));
                } else {
                    out = makeRuntimeString(context, __str__Value(context, lhs) + __str__Value(context, rhs));
                }
                writeRegister(0, out);
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (lhs.isInt() && rhs.isInt()) {
                frame.stack[frame.stackTop - 1] = Value::Int(lhs.asInt() + rhs.asInt());
            } else if (isNumericValue(lhs) && isNumericValue(rhs)) {
                frame.stack[frame.stackTop - 1] = Value::Float(toDouble(lhs) + toDouble(rhs));
            } else {
                frame.stack[frame.stackTop - 1] = makeRuntimeString(context, __str__Value(context, lhs) + __str__Value(context, rhs));
            }
            break;
        }
        case OpCode::Sub: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (lhs.isInt() && rhs.isInt()) {
                    writeRegister(0, Value::Int(lhs.asInt() - rhs.asInt()));
                } else if (isNumericValue(lhs) && isNumericValue(rhs)) {
                    writeRegister(0, Value::Float(toDouble(lhs) - toDouble(rhs)));
                } else {
                    throw std::runtime_error("Sub expects numeric operands");
                }
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (lhs.isInt() && rhs.isInt()) {
                frame.stack[frame.stackTop - 1] = Value::Int(lhs.asInt() - rhs.asInt());
            } else if (isNumericValue(lhs) && isNumericValue(rhs)) {
                frame.stack[frame.stackTop - 1] = Value::Float(toDouble(lhs) - toDouble(rhs));
            } else {
                throw std::runtime_error("Sub expects numeric operands");
            }
            break;
        }
        case OpCode::Mul: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (lhs.isInt() && rhs.isInt()) {
                    writeRegister(0, Value::Int(lhs.asInt() * rhs.asInt()));
                } else if (isNumericValue(lhs) && isNumericValue(rhs)) {
                    writeRegister(0, Value::Float(toDouble(lhs) * toDouble(rhs)));
                } else {
                    throw std::runtime_error("Mul expects numeric operands");
                }
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (lhs.isInt() && rhs.isInt()) {
                frame.stack[frame.stackTop - 1] = Value::Int(lhs.asInt() * rhs.asInt());
            } else if (isNumericValue(lhs) && isNumericValue(rhs)) {
                frame.stack[frame.stackTop - 1] = Value::Float(toDouble(lhs) * toDouble(rhs));
            } else {
                throw std::runtime_error("Mul expects numeric operands");
            }
            break;
        }
        case OpCode::Div: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                    throw std::runtime_error("Div expects numeric operands");
                }
                const double divisor = toDouble(rhs);
                if (std::abs(divisor) <= std::numeric_limits<double>::epsilon()) {
                    throw std::runtime_error("Division by zero");
                }
                writeRegister(0, Value::Float(toDouble(lhs) / divisor));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                throw std::runtime_error("Div expects numeric operands");
            }
            const double divisor = toDouble(rhs);
            if (std::abs(divisor) <= std::numeric_limits<double>::epsilon()) {
                throw std::runtime_error("Division by zero");
            }
            frame.stack[frame.stackTop - 1] = Value::Float(toDouble(lhs) / divisor);
            break;
        }
        case OpCode::FloorDiv: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                    throw std::runtime_error("FloorDiv expects numeric operands");
                }
                const double divisor = toDouble(rhs);
                if (std::abs(divisor) <= std::numeric_limits<double>::epsilon()) {
                    throw std::runtime_error("Division by zero");
                }
                writeRegister(0, Value::Int(static_cast<std::int64_t>(std::floor(toDouble(lhs) / divisor))));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                throw std::runtime_error("FloorDiv expects numeric operands");
            }
            const double divisor = toDouble(rhs);
            if (std::abs(divisor) <= std::numeric_limits<double>::epsilon()) {
                throw std::runtime_error("Division by zero");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(static_cast<std::int64_t>(std::floor(toDouble(lhs) / divisor)));
            break;
        }
        case OpCode::Mod: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (lhs.isInt() && rhs.isInt()) {
                    if (rhs.asInt() == 0) {
                        throw std::runtime_error("Modulo by zero");
                    }
                    writeRegister(0, Value::Int(lhs.asInt() % rhs.asInt()));
                } else if (isNumericValue(lhs) && isNumericValue(rhs)) {
                    const double divisor = toDouble(rhs);
                    if (std::abs(divisor) <= std::numeric_limits<double>::epsilon()) {
                        throw std::runtime_error("Modulo by zero");
                    }
                    writeRegister(0, Value::Float(std::fmod(toDouble(lhs), divisor)));
                } else {
                    throw std::runtime_error("Mod expects numeric operands");
                }
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (lhs.isInt() && rhs.isInt()) {
                if (rhs.asInt() == 0) {
                    throw std::runtime_error("Modulo by zero");
                }
                frame.stack[frame.stackTop - 1] = Value::Int(lhs.asInt() % rhs.asInt());
            } else if (isNumericValue(lhs) && isNumericValue(rhs)) {
                const double divisor = toDouble(rhs);
                if (std::abs(divisor) <= std::numeric_limits<double>::epsilon()) {
                    throw std::runtime_error("Modulo by zero");
                }
                frame.stack[frame.stackTop - 1] = Value::Float(std::fmod(toDouble(lhs), divisor));
            } else {
                throw std::runtime_error("Mod expects numeric operands");
            }
            break;
        }
        case OpCode::Pow: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                    throw std::runtime_error("Pow expects numeric operands");
                }
                writeRegister(0, Value::Float(std::pow(toDouble(lhs), toDouble(rhs))));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                throw std::runtime_error("Pow expects numeric operands");
            }
            frame.stack[frame.stackTop - 1] = Value::Float(std::pow(toDouble(lhs), toDouble(rhs)));
            break;
        }
        case OpCode::LessThan: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                    throw std::runtime_error("LessThan expects numeric operands");
                }
                writeRegister(0, Value::Int(toDouble(lhs) < toDouble(rhs) ? 1 : 0));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                throw std::runtime_error("LessThan expects numeric operands");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(toDouble(lhs) < toDouble(rhs) ? 1 : 0);
            break;
        }
        case OpCode::GreaterThan: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                    throw std::runtime_error("GreaterThan expects numeric operands");
                }
                writeRegister(0, Value::Int(toDouble(lhs) > toDouble(rhs) ? 1 : 0));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                throw std::runtime_error("GreaterThan expects numeric operands");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(toDouble(lhs) > toDouble(rhs) ? 1 : 0);
            break;
        }
        case OpCode::Equal: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                writeRegister(0, Value::Int(valueEquals(context, lhs, rhs) ? 1 : 0));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            frame.stack[frame.stackTop - 1] = Value::Int(valueEquals(context, lhs, rhs) ? 1 : 0);
            break;
        }
        case OpCode::NotEqual: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                writeRegister(0, Value::Int(valueEquals(context, lhs, rhs) ? 0 : 1));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            frame.stack[frame.stackTop - 1] = Value::Int(valueEquals(context, lhs, rhs) ? 0 : 1);
            break;
        }
        case OpCode::LessEqual: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                    throw std::runtime_error("LessEqual expects numeric operands");
                }
                writeRegister(0, Value::Int(toDouble(lhs) <= toDouble(rhs) ? 1 : 0));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                throw std::runtime_error("LessEqual expects numeric operands");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(toDouble(lhs) <= toDouble(rhs) ? 1 : 0);
            break;
        }
        case OpCode::GreaterEqual: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                    throw std::runtime_error("GreaterEqual expects numeric operands");
                }
                writeRegister(0, Value::Int(toDouble(lhs) >= toDouble(rhs) ? 1 : 0));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (!isNumericValue(lhs) || !isNumericValue(rhs)) {
                throw std::runtime_error("GreaterEqual expects numeric operands");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(toDouble(lhs) >= toDouble(rhs) ? 1 : 0);
            break;
        }
        case OpCode::Is: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                // Identity check: same object pointer or same value representation
                const bool same = (lhs.type == rhs.type && lhs.payload == rhs.payload);
                writeRegister(0, Value::Int(same ? 1 : 0));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            const bool same = (lhs.type == rhs.type && lhs.payload == rhs.payload);
            frame.stack[frame.stackTop - 1] = Value::Int(same ? 1 : 0);
            break;
        }
        case OpCode::IsNot: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                const bool same = (lhs.type == rhs.type && lhs.payload == rhs.payload);
                writeRegister(0, Value::Int(same ? 0 : 1));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            const bool same = (lhs.type == rhs.type && lhs.payload == rhs.payload);
            frame.stack[frame.stackTop - 1] = Value::Int(same ? 0 : 1);
            break;
        }
        case OpCode::BitwiseAnd: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (!lhs.isInt() || !rhs.isInt()) {
                    throw std::runtime_error("BitwiseAnd expects integer operands");
                }
                writeRegister(0, Value::Int(lhs.asInt() & rhs.asInt()));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (!lhs.isInt() || !rhs.isInt()) {
                throw std::runtime_error("BitwiseAnd expects integer operands");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(lhs.asInt() & rhs.asInt());
            break;
        }
        case OpCode::BitwiseOr: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (!lhs.isInt() || !rhs.isInt()) {
                    throw std::runtime_error("BitwiseOr expects integer operands");
                }
                writeRegister(0, Value::Int(lhs.asInt() | rhs.asInt()));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (!lhs.isInt() || !rhs.isInt()) {
                throw std::runtime_error("BitwiseOr expects integer operands");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(lhs.asInt() | rhs.asInt());
            break;
        }
        case OpCode::BitwiseXor: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (!lhs.isInt() || !rhs.isInt()) {
                    throw std::runtime_error("BitwiseXor expects integer operands");
                }
                writeRegister(0, Value::Int(lhs.asInt() ^ rhs.asInt()));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (!lhs.isInt() || !rhs.isInt()) {
                throw std::runtime_error("BitwiseXor expects integer operands");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(lhs.asInt() ^ rhs.asInt());
            break;
        }
        case OpCode::ShiftLeft: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (!lhs.isInt() || !rhs.isInt()) {
                    throw std::runtime_error("ShiftLeft expects integer operands");
                }
                writeRegister(0, Value::Int(lhs.asInt() << rhs.asInt()));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (!lhs.isInt() || !rhs.isInt()) {
                throw std::runtime_error("ShiftLeft expects integer operands");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(lhs.asInt() << rhs.asInt());
            break;
        }
        case OpCode::ShiftRight: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                if (!lhs.isInt() || !rhs.isInt()) {
                    throw std::runtime_error("ShiftRight expects integer operands");
                }
                writeRegister(0, Value::Int(lhs.asInt() >> rhs.asInt()));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            if (!lhs.isInt() || !rhs.isInt()) {
                throw std::runtime_error("ShiftRight expects integer operands");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(lhs.asInt() >> rhs.asInt());
            break;
        }
        case OpCode::LogicalAnd: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                const bool lhsTruthy = isTruthy(context, lhs);
                const bool rhsTruthy = isTruthy(context, rhs);
                writeRegister(0, Value::Int((lhsTruthy && rhsTruthy) ? 1 : 0));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            const bool lhsTruthy = isTruthy(context, lhs);
            const bool rhsTruthy = isTruthy(context, rhs);
            frame.stack[frame.stackTop - 1] = Value::Int((lhsTruthy && rhsTruthy) ? 1 : 0);
            break;
        }
        case OpCode::LogicalOr: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value lhs = resolveSlotValue(ins.aSlotType, ins.a);
                const Value rhs = resolveSlotValue(ins.bSlotType, ins.b);
                const bool lhsTruthy = isTruthy(context, lhs);
                const bool rhsTruthy = isTruthy(context, rhs);
                writeRegister(0, Value::Int((lhsTruthy || rhsTruthy) ? 1 : 0));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value rhs = frame.stack[frame.stackTop - 1];
            const Value lhs = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            const bool lhsTruthy = isTruthy(context, lhs);
            const bool rhsTruthy = isTruthy(context, rhs);
            frame.stack[frame.stackTop - 1] = Value::Int((lhsTruthy || rhsTruthy) ? 1 : 0);
            break;
        }
        case OpCode::In: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value element = resolveSlotValue(ins.aSlotType, ins.a);
                const Value container = resolveSlotValue(ins.bSlotType, ins.b);
                bool found = false;
                if (container.isRef()) {
                    Object* obj = container.asRef();
                    if (auto* list = dynamic_cast<ListObject*>(obj)) {
                        for (const auto& item : list->data()) {
                            if (valueEquals(context, element, item)) {
                                found = true;
                                break;
                            }
                        }
                    } else if (auto* dict = dynamic_cast<DictObject*>(obj)) {
                        if (element.isInt()) {
                            found = dict->data().contains(element.asInt());
                        }
                    } else if (auto* tuple = dynamic_cast<TupleObject*>(obj)) {
                        for (const auto& item : tuple->data()) {
                            if (valueEquals(context, element, item)) {
                                found = true;
                                break;
                            }
                        }
                    } else if (auto* inst = dynamic_cast<ScriptInstanceObject*>(obj)) {
                        const std::string attrName = __str__Value(context, element);
                        found = inst->fields().contains(attrName);
                    } else {
                        throw std::runtime_error("'in' operator expects list, dict, tuple, or object");
                    }
                } else if (container.isRef()) {
                    if (auto* mod = dynamic_cast<ModuleObject*>(container.asRef())) {
                        const std::string name = __str__Value(context, element);
                        found = mod->exports().contains(name);
                    }
                } else {
                    throw std::runtime_error("'in' operator expects container type");
                }
                writeRegister(0, Value::Int(found ? 1 : 0));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value container = frame.stack[frame.stackTop - 1];
            const Value element = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            bool found = false;
            if (container.isRef()) {
                Object* obj = container.asRef();
                if (auto* list = dynamic_cast<ListObject*>(obj)) {
                    for (const auto& item : list->data()) {
                        if (valueEquals(context, element, item)) {
                            found = true;
                            break;
                        }
                    }
                } else if (auto* dict = dynamic_cast<DictObject*>(obj)) {
                    if (element.isInt()) {
                        found = dict->data().contains(element.asInt());
                    }
                } else if (auto* tuple = dynamic_cast<TupleObject*>(obj)) {
                    for (const auto& item : tuple->data()) {
                        if (valueEquals(context, element, item)) {
                            found = true;
                            break;
                        }
                    }
                } else if (auto* inst = dynamic_cast<ScriptInstanceObject*>(obj)) {
                    const std::string attrName = __str__Value(context, element);
                    found = inst->fields().contains(attrName);
                } else {
                    throw std::runtime_error("'in' operator expects list, dict, tuple, or object");
                }
            } else if (container.isRef()) {
                if (auto* mod = dynamic_cast<ModuleObject*>(container.asRef())) {
                    const std::string name = __str__Value(context, element);
                    found = mod->exports().contains(name);
                }
            } else {
                throw std::runtime_error("'in' operator expects container type");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(found ? 1 : 0);
            break;
        }
        case OpCode::NotIn: {
            if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
                const Value element = resolveSlotValue(ins.aSlotType, ins.a);
                const Value container = resolveSlotValue(ins.bSlotType, ins.b);
                bool found = false;
                if (container.isRef()) {
                    Object* obj = container.asRef();
                    if (auto* list = dynamic_cast<ListObject*>(obj)) {
                        for (const auto& item : list->data()) {
                            if (valueEquals(context, element, item)) {
                                found = true;
                                break;
                            }
                        }
                    } else if (auto* dict = dynamic_cast<DictObject*>(obj)) {
                        if (element.isInt()) {
                            found = dict->data().contains(element.asInt());
                        }
                    } else if (auto* tuple = dynamic_cast<TupleObject*>(obj)) {
                        for (const auto& item : tuple->data()) {
                            if (valueEquals(context, element, item)) {
                                found = true;
                                break;
                            }
                        }
                    } else if (auto* inst = dynamic_cast<ScriptInstanceObject*>(obj)) {
                        const std::string attrName = __str__Value(context, element);
                        found = inst->fields().contains(attrName);
                    } else {
                        throw std::runtime_error("'not in' operator expects list, dict, tuple, or object");
                    }
                } else if (container.isRef()) {
                    if (auto* mod = dynamic_cast<ModuleObject*>(container.asRef())) {
                        const std::string name = __str__Value(context, element);
                        found = mod->exports().contains(name);
                    }
                } else {
                    throw std::runtime_error("'not in' operator expects container type");
                }
                writeRegister(0, Value::Int(found ? 0 : 1));
                break;
            }
            if (frame.stackTop < 2) {
                throw std::runtime_error("Stack underflow");
            }
            const Value container = frame.stack[frame.stackTop - 1];
            const Value element = frame.stack[frame.stackTop - 2];
            --frame.stackTop;
            bool found = false;
            if (container.isRef()) {
                Object* obj = container.asRef();
                if (auto* list = dynamic_cast<ListObject*>(obj)) {
                    for (const auto& item : list->data()) {
                        if (valueEquals(context, element, item)) {
                            found = true;
                            break;
                        }
                    }
                } else if (auto* dict = dynamic_cast<DictObject*>(obj)) {
                    if (element.isInt()) {
                        found = dict->data().contains(element.asInt());
                    }
                } else if (auto* tuple = dynamic_cast<TupleObject*>(obj)) {
                    for (const auto& item : tuple->data()) {
                        if (valueEquals(context, element, item)) {
                            found = true;
                            break;
                        }
                    }
                } else if (auto* inst = dynamic_cast<ScriptInstanceObject*>(obj)) {
                    const std::string attrName = __str__Value(context, element);
                    found = inst->fields().contains(attrName);
                } else {
                    throw std::runtime_error("'not in' operator expects list, dict, tuple, or object");
                }
            } else if (container.isRef()) {
                if (auto* mod = dynamic_cast<ModuleObject*>(container.asRef())) {
                    const std::string name = __str__Value(context, element);
                    found = mod->exports().contains(name);
                }
            } else {
                throw std::runtime_error("'not in' operator expects container type");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(found ? 0 : 1);
            break;
        }
        case OpCode::Negate: {
            if (ins.aSlotType != SlotType::None) {
                const Value operand = resolveSlotValue(ins.aSlotType, ins.a);
                if (operand.isInt()) {
                    writeRegister(0, Value::Int(-operand.asInt()));
                } else if (operand.isFloat()) {
                    writeRegister(0, Value::Float(-operand.asFloat()));
                } else {
                    throw std::runtime_error("Negate expects numeric operand");
                }
                break;
            }
            if (frame.stackTop == 0) {
                throw std::runtime_error("Stack underflow");
            }
            const Value operand = frame.stack[frame.stackTop - 1];
            if (operand.isInt()) {
                frame.stack[frame.stackTop - 1] = Value::Int(-operand.asInt());
            } else if (operand.isFloat()) {
                frame.stack[frame.stackTop - 1] = Value::Float(-operand.asFloat());
            } else {
                throw std::runtime_error("Negate expects numeric operand");
            }
            break;
        }
        case OpCode::Not: {
            if (ins.aSlotType != SlotType::None) {
                const Value operand = resolveSlotValue(ins.aSlotType, ins.a);
                writeRegister(0, Value::Int(toBoolInt(operand) == 0 ? 1 : 0));
                break;
            }
            if (frame.stackTop == 0) {
                throw std::runtime_error("Stack underflow");
            }
            const Value operand = frame.stack[frame.stackTop - 1];
            frame.stack[frame.stackTop - 1] = Value::Int(toBoolInt(operand) == 0 ? 1 : 0);
            break;
        }
        case OpCode::BitwiseNot: {
            if (ins.aSlotType != SlotType::None) {
                const Value operand = resolveSlotValue(ins.aSlotType, ins.a);
                if (!operand.isInt()) {
                    throw std::runtime_error("BitwiseNot expects integer operand");
                }
                writeRegister(0, Value::Int(~operand.asInt()));
                break;
            }
            if (frame.stackTop == 0) {
                throw std::runtime_error("Stack underflow");
            }
            const Value operand = frame.stack[frame.stackTop - 1];
            if (!operand.isInt()) {
                throw std::runtime_error("BitwiseNot expects integer operand");
            }
            frame.stack[frame.stackTop - 1] = Value::Int(~operand.asInt());
            break;
        }
        case OpCode::Jump:
            frame.ip = static_cast<std::size_t>(ins.a);
            break;
        case OpCode::JumpIfFalse: {
            if (frame.stackTop == 0) {
                throw std::runtime_error("Stack underflow");
            }
            --frame.stackTop;
            const Value cond = frame.stack[frame.stackTop];
            if (toBoolInt(cond) == 0) {
                frame.ip = static_cast<std::size_t>(ins.a);
            }
            break;
        }
        case OpCode::JumpIfFalseReg: {
            const Value cond = normalizeRuntimeValue(context,
                                                     functionType_,
                                                     classType_,
                                                     nativeFunctionType_,
                                                     moduleType_,
                                                     hosts_,
                                                     frameModule,
                                                     frame.registerValue,
                                                     false);
            if (toBoolInt(cond) == 0) {
                frame.ip = static_cast<std::size_t>(ins.a);
            }
            break;
        }
        case OpCode::CallHost: {
            collectArgs(frame.stack, frame.stackTop, static_cast<std::size_t>(ins.b), argScratch);
            const auto& name = frameModule->strings.at(ins.a);
            const std::size_t callerFrameIndex = context.frames.size() - 1;
            VmHostContext hostContext(*this, context);
            const Value result = hosts_.invoke(name, hostContext, argScratch);
            if (callerFrameIndex < context.frames.size()) {
                pushRaw(context.frames[callerFrameIndex].stack, context.frames[callerFrameIndex].stackTop, result);
            }
            break;
        }
        case OpCode::CallFunc: {
            collectArgs(frame.stack, frame.stackTop, static_cast<std::size_t>(ins.b), argScratch);
            pushCallFrame(context, frameModule, static_cast<std::size_t>(ins.a), argScratch);
            break;
        }
        case OpCode::NewInstance: {
            collectArgs(frame.stack, frame.stackTop, static_cast<std::size_t>(ins.b), argScratch);
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
            ctorInvokeArgs.reserve(argScratch.size() + 1);
            ctorInvokeArgs.push_back(instanceRef);
            ctorInvokeArgs.insert(ctorInvokeArgs.end(), argScratch.begin(), argScratch.end());
            pushCallFrame(context,
                          frameModule,
                          ctorFunctionIndex,
                          ctorInvokeArgs,
                          true,
                          instanceRef);
            break;
        }
        case OpCode::LoadAttr: {
            const Value selfRef = popRaw(frame.stack, frame.stackTop);
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
                pushRaw(frame.stack, frame.stackTop, it->second);
            } else if (auto* moduleObj = dynamic_cast<ModuleObject*>(&object)) {
                auto exportIt = moduleObj->exports().find(attrName);
                if (exportIt != moduleObj->exports().end()) {
                    pushRaw(frame.stack, frame.stackTop, exportIt->second);
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
                            pushRaw(frame.stack, frame.stackTop, globalValue);
                            goto load_attr_done;
                        }
                    }

                    for (std::size_t i = 0; i < modulePin->functions.size(); ++i) {
                        const auto& fn = modulePin->functions[i];
                        if (fn.name == attrName) {
                            Value functionRef = makeFunctionObject(context, functionType_, i, modulePin);
                            rememberWriteBarrier(context, *moduleObj, functionRef);
                            moduleObj->exports()[attrName] = functionRef;
                            pushRaw(frame.stack, frame.stackTop, functionRef);
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
                            pushRaw(frame.stack, frame.stackTop, classRef);
                            goto load_attr_done;
                        }
                    }
                }

                pushRaw(frame.stack, frame.stackTop, object.getType().getMember(object, attrName));
            } else {
                pushRaw(frame.stack, frame.stackTop, object.getType().getMember(object, attrName));
            }
load_attr_done:
            break;
        }
        case OpCode::StoreAttr: {
            const Value assigned = popRaw(frame.stack, frame.stackTop);
            const Value selfRef = popRaw(frame.stack, frame.stackTop);
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
                pushRaw(frame.stack, frame.stackTop, normalized);
            } else {
                rememberWriteBarrier(context, object, normalized);
                pushRaw(frame.stack, frame.stackTop, object.getType().setMember(object, attrName, normalized));
            }
            break;
        }
        case OpCode::CallMethod: {
            collectArgs(frame.stack, frame.stackTop, static_cast<std::size_t>(ins.b), argScratch);
            const Value selfRef = popRaw(frame.stack, frame.stackTop);
            Object& object = getObject(context, selfRef);
            const auto& methodName = frameModule->strings.at(ins.a);

            const auto makeString = [&](const std::string& text) {
                return makeRuntimeString(context, text);
            };
            const auto valueStr = [&](const Value& nested) {
                return __str__Value(context, nested);
            };

            if (auto* moduleObj = dynamic_cast<ModuleObject*>(&object)) {
                auto exportIt = moduleObj->exports().find(methodName);
                if (exportIt != moduleObj->exports().end()) {
                    const auto& exportModulePin = moduleObj->modulePin();
                    const Value callable = normalizeRuntimeValue(context,
                                                                 functionType_,
                                                                 classType_,
                                                                 nativeFunctionType_,
                                                                 moduleType_,
                                                                 hosts_,
                                                                 exportModulePin,
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
                    if (tryInvokeScriptCallable(callableObject,
                                                exportModulePin,
                                                argScratch,
                                                "Module export callable is missing module binding: " + methodName)) {
                        goto call_method_done;
                    }

                    if (tryInvokeClassOrNativeCallable(callableObject, argScratch)) {
                        goto call_method_done;
                    }

                    throw std::runtime_error("Module export is not function or class: " + methodName);
                }

                const auto& modulePin = moduleObj->modulePin();
                if (!modulePin) {
                    throw std::runtime_error("Module object is not loaded: " + moduleObj->moduleName());
                }

                if (tryInvokeModuleNamedCallable(modulePin, methodName, argScratch)) {
                    goto call_method_done;
                }

                throw std::runtime_error("Script function not found: " + methodName);
            }

            if (auto* list = dynamic_cast<ListObject*>(&object)) {
                if (methodName == "push" && !argScratch.empty()) {
                    rememberWriteBarrier(context, *list, argScratch[0]);
                } else if (methodName == "set" && argScratch.size() >= 2) {
                    rememberWriteBarrier(context, *list, argScratch[1]);
                }
            } else if (auto* dict = dynamic_cast<DictObject*>(&object)) {
                if (methodName == "set" && argScratch.size() >= 2) {
                    rememberWriteBarrier(context, *dict, argScratch[1]);
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
                    if (tryInvokeScriptCallable(callableObject,
                                                callValueModule,
                                                argScratch,
                                                "Object property callable is missing module binding: " + methodName)) {
                        break;
                    }

                    if (tryInvokeClassOrNativeCallable(callableObject, argScratch)) {
                        break;
                    }

                    throw std::runtime_error("Object property is not a supported callable object: " + methodName);
                }

                std::size_t classMethodIndex = 0;
                auto methodModule = instance->modulePin() ? instance->modulePin() : frameModule;
                if (tryFindClassMethodInModule(*methodModule, instance->classIndex(), methodName, classMethodIndex)) {
                    std::vector<Value> methodArgs;
                    methodArgs.reserve(argScratch.size() + 1);
                    methodArgs.push_back(selfRef);
                    methodArgs.insert(methodArgs.end(), argScratch.begin(), argScratch.end());
                    pushCallFrame(context,
                                  methodModule,
                                  classMethodIndex,
                                  methodArgs);
                    break;
                }

                pushRaw(frame.stack, frame.stackTop, object.getType().callMethod(object,
                                                                  methodName,
                                                                  argScratch,
                                                                  makeString,
                                                                  valueStr));
            } else {
                pushRaw(frame.stack, frame.stackTop, object.getType().callMethod(object,
                                                                  methodName,
                                                                  argScratch,
                                                                  makeString,
                                                                  valueStr));
            }
call_method_done:
            break;
        }
        case OpCode::CallValue: {
            collectArgs(frame.stack, frame.stackTop, static_cast<std::size_t>(ins.a), argScratch);
            Value callable = popRaw(frame.stack, frame.stackTop);
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
            if (tryInvokeScriptCallable(callableObject,
                                        frameModule,
                                        argScratch,
                                        "Callable object is missing module binding")) {
                break;
            }

            if (tryInvokeClassOrNativeCallable(callableObject, argScratch)) {
                break;
            }

            throw std::runtime_error("Attempted to call a non-function object");
        }
        case OpCode::CallIntrinsic:
            throw std::runtime_error("CallIntrinsic is deprecated. Use Type exported methods.");
        case OpCode::SpawnFunc: {
            collectArgs(frame.stack, frame.stackTop, static_cast<std::size_t>(ins.b), argScratch);
            const auto funcName = frameModule->functions.at(ins.a).name;
            const auto module = frameModule;
            const auto* hosts = &hosts_;
            auto& tasks = tasks_;
            const std::vector<Value> asyncArgs = argScratch;
            const std::int64_t handle = tasks_.enqueue([module, hosts, &tasks, funcName, asyncArgs]() {
                VirtualMachine vm(module, *hosts, tasks);
                return vm.runFunction(funcName, asyncArgs);
            });
            pushRaw(frame.stack, frame.stackTop, Value::Int(handle));
            break;
        }
        case OpCode::Await: {
            const auto handle = popRaw(frame.stack, frame.stackTop).asInt();
            pushRaw(frame.stack, frame.stackTop, tasks_.await(handle));
            break;
        }
        case OpCode::MakeList: {
            const std::size_t count = static_cast<std::size_t>(ins.a);
            collectArgs(frame.stack, frame.stackTop, count, argScratch);
            pushRaw(frame.stack, frame.stackTop, emplaceObject(context, std::make_unique<ListObject>(listType_, argScratch)));
            break;
        }
        case OpCode::MakeDict: {
            const std::size_t pairCount = static_cast<std::size_t>(ins.a);
            if (frame.stackTop < pairCount * 2) {
                throw std::runtime_error("Not enough stack values for dict literal");
            }
            std::unordered_map<std::int64_t, Value> values;
            for (std::size_t i = 0; i < pairCount; ++i) {
                const Value value = popRaw(frame.stack, frame.stackTop);
                const Value key = popRaw(frame.stack, frame.stackTop);
                values[key.asInt()] = value;
            }
            pushRaw(frame.stack, frame.stackTop, emplaceObject(context, std::make_unique<DictObject>(dictType_, std::move(values))));
            break;
        }
        case OpCode::Sleep:
            std::this_thread::sleep_for(std::chrono::milliseconds(ins.a));
            break;
        case OpCode::Yield:
            std::this_thread::yield();
            break;
        case OpCode::Return: {
            Value ret = Value::Nil();
            if (frame.stackTop > 0) {
                --frame.stackTop;
                ret = frame.stack[frame.stackTop];
            }
            if (frame.replaceReturnWithInstance) {
                ret = frame.constructorInstance;
            }
            context.frames.pop_back();
            if (context.frames.empty()) {
                context.returnValue = ret;
                return true;
            }
            pushRaw(context.frames.back().stack, context.frames.back().stackTop, ret);
            break;
        }
        case OpCode::Pop:
            if (frame.stackTop == 0) {
                throw std::runtime_error("Stack underflow");
            }
            --frame.stackTop;
            break;
        case OpCode::MoveLocalToReg:
            writeRegister(ins.b, resolveSlotValue(SlotType::Local, ins.a));
            break;
        case OpCode::MoveNameToReg: {
            const auto& symbolName = frameModule->strings.at(ins.a);
            writeRegister(ins.b, resolveRuntimeName(context,
                                                    functionType_,
                                                    classType_,
                                                    nativeFunctionType_,
                                                    moduleType_,
                                                    hosts_,
                                                    frameModule,
                                                    symbolName));
            break;
        }
        case OpCode::ConstToReg:
            writeRegister(ins.b, normalizeRuntimeValue(context,
                                                       functionType_,
                                                       classType_,
                                                       nativeFunctionType_,
                                                       moduleType_,
                                                       hosts_,
                                                       frameModule,
                                                       frameModule->constants.at(ins.a),
                                                       true));
            break;
        case OpCode::LoadConst:
            {
                const Value loaded = normalizeRuntimeValue(context,
                                                           functionType_,
                                                           classType_,
                                                           nativeFunctionType_,
                                                           moduleType_,
                                                           hosts_,
                                                           frameModule,
                                                           frameModule->constants.at(ins.a),
                                                           true);
                Value& localValue = frame.locals.at(ins.b);
                if (localValue.isRef()) {
                    Object* localObject = localValue.asRef();
                    if (localObject) {
                        if (auto* cell = dynamic_cast<UpvalueCellObject*>(localObject)) {
                            rememberWriteBarrier(context, *cell, loaded);
                            cell->value() = loaded;
                            break;
                        }
                    }
                }
                localValue = loaded;
            }
            break;
        case OpCode::PushReg:
            pushRaw(frame.stack, frame.stackTop, readRegister(ins.a));
            break;
        case OpCode::CaptureLocal: {
            // Convert a normal local into a shared upvalue cell on first capture.
            // Later closures and the current frame both observe the same cell value.
            Value& localValue = frame.locals.at(ins.a);
            if (!(localValue.isRef() && localValue.asRef() != nullptr && dynamic_cast<UpvalueCellObject*>(localValue.asRef()) != nullptr)) {
                localValue = emplaceObject(context, std::make_unique<UpvalueCellObject>(upvalueCellType_, localValue));
            }
            pushRaw(frame.stack, frame.stackTop, localValue);
            break;
        }
        case OpCode::PushCapture:
        case OpCode::LoadCapture: {
            // Read captured value by-reference through its upvalue cell.
            if (ins.a < 0 || static_cast<std::size_t>(ins.a) >= frame.captures.size()) {
                throw std::runtime_error("Capture index out of range");
            }
            const Value captureRef = frame.captures[static_cast<std::size_t>(ins.a)];
            Object& object = getObject(context, captureRef);
            auto* cell = dynamic_cast<UpvalueCellObject*>(&object);
            if (!cell) {
                throw std::runtime_error("Capture is not an upvalue cell");
            }
            pushRaw(frame.stack, frame.stackTop, cell->value());
            break;
        }
        case OpCode::StoreCapture: {
            // Write captured value by-reference through the same upvalue cell.
            if (ins.a < 0 || static_cast<std::size_t>(ins.a) >= frame.captures.size()) {
                throw std::runtime_error("Capture index out of range");
            }
            const Value value = popRaw(frame.stack, frame.stackTop);
            const Value captureRef = frame.captures[static_cast<std::size_t>(ins.a)];
            Object& object = getObject(context, captureRef);
            auto* cell = dynamic_cast<UpvalueCellObject*>(&object);
            if (!cell) {
                throw std::runtime_error("Capture is not an upvalue cell");
            }
            rememberWriteBarrier(context, *cell, value);
            cell->value() = value;
            break;
        }
        case OpCode::MakeClosure: {
            const std::size_t captureCount = static_cast<std::size_t>(ins.b);
            if (frame.stackTop < captureCount) {
                throw std::runtime_error("Not enough captured values on stack");
            }
            std::vector<Value> captures;
            captures.resize(captureCount);
            for (std::size_t i = 0; i < captureCount; ++i) {
                captures[captureCount - 1 - i] = popRaw(frame.stack, frame.stackTop);
            }
            const Value fnRef = makeLambdaObject(context,
                                                 lambdaType_,
                                                 static_cast<std::size_t>(ins.a),
                                                 frameModule,
                                                 std::move(captures));
            pushRaw(frame.stack, frame.stackTop, fnRef);
            break;
        }
        case OpCode::StoreLocalFromReg:
            {
                Value& localValue = frame.locals.at(ins.a);
                if (localValue.isRef()) {
                    Object* localObject = localValue.asRef();
                    if (localObject) {
                        if (auto* cell = dynamic_cast<UpvalueCellObject*>(localObject)) {
                            rememberWriteBarrier(context, *cell, readRegister(ins.b));
                            cell->value() = readRegister(ins.b);
                            break;
                        }
                    }
                }
                localValue = readRegister(ins.b);
            }
            break;
        case OpCode::StoreNameFromReg: {
            const auto& symbolName = frameModule->strings.at(ins.a);
            storeRuntimeGlobal(context, frameModule, symbolName, readRegister(ins.b));
            break;
        }
        }

        runGcSlice(context, context.gc.sliceBudgetObjects);
    }

    return context.frames.empty();
}

Value VirtualMachine::runFunction(const std::string& functionName, const std::vector<Value>& args) {
    ExecutionContext ctx;
    ctx.modulePin = module_;
    ctx.stringPool = module_->strings;
    ensureModuleInitialized(ctx, module_);
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


