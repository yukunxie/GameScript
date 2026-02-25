#include "gs/binding.hpp"
#include "gs/bound_class_type.hpp"
#include "gs/global.hpp"
#include "gs/type_system/module_type.hpp"
#include "gs/type_system/native_function_type.hpp"
#include "gs/type_system/type_base.hpp"

#include <stdexcept>
#include <typeindex>

namespace gs {

// ============================================================================
// Global Type Registry for Custom Native Types
// ============================================================================

namespace {
    // Map from std::type_index to dynamically allocated Type*
    // This allows TypeConverter<T>::toValue to find the correct Type for wrapping
    std::unordered_map<std::type_index, const Type*> g_nativeTypeRegistry;
}

void registerNativeType(const std::type_info& typeInfo, const Type& type) {
    g_nativeTypeRegistry[std::type_index(typeInfo)] = &type;
}

const Type* getNativeType(const std::type_info& typeInfo) {
    auto it = g_nativeTypeRegistry.find(std::type_index(typeInfo));
    if (it == g_nativeTypeRegistry.end()) {
        return nullptr;
    }
    return it->second;
}

// ============================================================================
// HostRegistry Implementation
// ============================================================================

HostRegistry::HostRegistry() {
    bindGlobalModule(*this);
}

void HostRegistry::bind(const std::string& name, HostFunction fn) {
    if (!fn) {
        throw std::runtime_error("Host function callback is empty: " + name);
    }

    BuiltinEntry entry;
    entry.kind = BuiltinEntry::Kind::Function;
    entry.function = std::move(fn);
    builtins_[name] = std::move(entry);
}

void HostRegistry::defineModule(const std::string& moduleName) {
    auto it = builtins_.find(moduleName);
    if (it != builtins_.end()) {
        if (it->second.kind != BuiltinEntry::Kind::Module) {
            throw std::runtime_error("Builtin name already used by function: " + moduleName);
        }
        return;
    }

    BuiltinEntry entry;
    entry.kind = BuiltinEntry::Kind::Module;
    builtins_[moduleName] = std::move(entry);
}

void HostRegistry::bindModuleFunction(const std::string& moduleName,
                                      const std::string& exportName,
                                      HostFunction fn) {
    if (!fn) {
        throw std::runtime_error("Module function callback is empty: " + moduleName + "." + exportName);
    }

    defineModule(moduleName);
    auto& entry = builtins_.at(moduleName);
    if (entry.kind != BuiltinEntry::Kind::Module) {
        throw std::runtime_error("Builtin is not a module: " + moduleName);
    }

    entry.moduleFunctions[exportName] = std::move(fn);
}

bool HostRegistry::has(const std::string& name) const {
    return builtins_.contains(name);
}

bool HostRegistry::hasModule(const std::string& name) const {
    auto it = builtins_.find(name);
    if (it == builtins_.end()) {
        return false;
    }
    return it->second.kind == BuiltinEntry::Kind::Module;
}

Value HostRegistry::invoke(const std::string& name, HostContext& context, const std::vector<Value>& args) const {
    auto it = builtins_.find(name);
    if (it == builtins_.end()) {
        throw std::runtime_error("Host function not found: " + name);
    }

    if (it->second.kind != BuiltinEntry::Kind::Function) {
        throw std::runtime_error("Builtin is not a function: " + name);
    }

    return it->second.function(context, args);
}

Value HostRegistry::resolveBuiltin(const std::string& name,
                                   HostContext& context,
                                   NativeFunctionType& nativeFunctionType,
                                   ModuleType& moduleType) const {
    auto it = builtins_.find(name);
    if (it == builtins_.end()) {
        throw std::runtime_error("Builtin not found: " + name);
    }

    const auto& entry = it->second;
    if (entry.kind == BuiltinEntry::Kind::Function) {
        return context.createObject(std::make_unique<NativeFunctionObject>(nativeFunctionType,
                                                                            name,
                                                                            entry.function));
    }

    auto moduleObject = std::make_unique<ModuleObject>(moduleType, name);
    moduleObject->exports()["__name__"] = context.createString(name);
    for (const auto& [exportName, callback] : entry.moduleFunctions) {
        moduleObject->exports()[exportName] = context.createObject(std::make_unique<NativeFunctionObject>(nativeFunctionType,
                                                                                                             exportName,
                                                                                                             callback));
    }
    return context.createObject(std::move(moduleObject));
}

// ============================================================================
// V2 API Implementation
// ============================================================================

// ClassBinder implementation
ClassBinder::ClassBinder(BindingContext& ctx, const std::string& className)
    : ctx_(ctx), className_(className) {}

ClassBinder::ClassBinder(ClassBinder&& other) noexcept
    : ctx_(other.ctx_),
      className_(std::move(other.className_)),
      constructorFunc_(std::move(other.constructorFunc_)),
      methods_(std::move(other.methods_)),
      propertyGetters_(std::move(other.propertyGetters_)),
      propertySetters_(std::move(other.propertySetters_)) {
    // Transferred ownership, prevent the moved-from object from registering
    other.constructorFunc_ = nullptr;
}

ClassBinder::~ClassBinder() {
    // Nothing to do here - registration happens immediately now
}

void ClassBinder::finalize() {
    // Register constructor as a global function
    if (constructorFunc_) {
        ctx_.registry().bind(className_, constructorFunc_);
        constructorFunc_ = nullptr;  // Prevent double-registration
    }
    
    // Register members and methods to BoundClassType
    if (typePtr_) {
        auto* boundType = dynamic_cast<BoundClassType*>(typePtr_);
        if (boundType) {
            // Register field getters
            for (const auto& [name, getter] : propertyGetters_) {
                boundType->registerGetter(name, getter);
            }
            
            // Register field setters
            for (const auto& [name, setter] : propertySetters_) {
                boundType->registerSetter(name, setter);
            }
            
            // Register methods
            for (const auto& [name, method] : methods_) {
                boundType->registerMethod(name, method);
            }
        }
    }
}

// BindingContext implementation
BindingContext::BindingContext(HostRegistry& registry)
    : registry_(registry) {}

// ScriptCallableInvoker implementation
ScriptCallableInvoker::ScriptCallableInvoker(HostContext& ctx, const Value& callable)
    : ctx_(ctx), callable_(callable) {
    if (callable_.isNil()) {
        throw std::runtime_error("Cannot create invoker for nil value");
    }
}

Value ScriptCallableInvoker::invokeInternal(const std::vector<Value>& args) {
    // TODO: Integrate with VM to actually call script functions
    // For now, this is a placeholder that needs VM integration
    throw std::runtime_error("ScriptCallableInvoker::invokeInternal not yet implemented - needs VM integration");
}

} // namespace gs
