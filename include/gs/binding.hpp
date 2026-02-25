#pragma once

#include "gs/bytecode.hpp"
#include "gs/type_system.hpp"  // Required for Object, Type

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace gs {

// Forward declarations
class NativeFunctionType;
class ModuleType;
class BindingContext;
class ClassBinder;

// ============================================================================
// HostContext (unchanged from original)
// ============================================================================

class HostContext {
public:
    virtual ~HostContext() = default;
    virtual Value createObject(std::unique_ptr<Object> object) = 0;
    virtual Value createString(const std::string& text) = 0;
    virtual Object& getObject(const Value& ref) = 0;
    virtual std::string __str__(const Value& value) = 0;
    virtual std::string typeName(const Value& value) = 0;
    virtual std::uint64_t objectId(const Value& ref) = 0;
    virtual Value collectGarbage(std::int64_t generation) = 0;
    virtual void ensureModuleInitialized(const Value& moduleRef) = 0;
    virtual bool tryGetCachedModuleObject(const std::string& moduleKey, Value& outModuleRef) = 0;
    virtual void cacheModuleObject(const std::string& moduleKey, const Value& moduleRef) = 0;
};

using HostFunction = std::function<Value(HostContext& context, const std::vector<Value>&)>;

// ============================================================================
// HostRegistry (unchanged from original)
// ============================================================================

class HostRegistry {
public:
    HostRegistry();

    void bind(const std::string& name, HostFunction fn);
    void defineModule(const std::string& moduleName);
    void bindModuleFunction(const std::string& moduleName,
                            const std::string& exportName,
                            HostFunction fn);
    bool has(const std::string& name) const;
    bool hasModule(const std::string& name) const;
    Value invoke(const std::string& name, HostContext& context, const std::vector<Value>& args) const;
    Value resolveBuiltin(const std::string& name,
                         HostContext& context,
                         NativeFunctionType& nativeFunctionType,
                         ModuleType& moduleType) const;

private:
    struct BuiltinEntry {
        enum class Kind {
            Function,
            Module
        };

        Kind kind{Kind::Function};
        HostFunction function;
        std::unordered_map<std::string, HostFunction> moduleFunctions;
    };

    std::unordered_map<std::string, BuiltinEntry> builtins_;
};

// ============================================================================
// Type Conversion System (New V2 API)
// ============================================================================

namespace detail {

// Type conversion traits
template<typename T>
struct TypeConverter {
    static T fromValue(HostContext& ctx, const Value& val);
    static Value toValue(HostContext& ctx, const T& val);
};

// Specializations for basic types
template<>
struct TypeConverter<std::int64_t> {
    static std::int64_t fromValue(HostContext& ctx, const Value& val) {
        return val.asInt();
    }
    static Value toValue(HostContext& ctx, std::int64_t val) {
        return Value::Int(val);
    }
};

template<>
struct TypeConverter<std::int32_t> {
    static std::int32_t fromValue(HostContext& ctx, const Value& val) {
        return static_cast<std::int32_t>(val.asInt());
    }
    static Value toValue(HostContext& ctx, std::int32_t val) {
        return Value::Int(static_cast<std::int64_t>(val));
    }
};

template<>
struct TypeConverter<std::uint32_t> {
    static std::uint32_t fromValue(HostContext& ctx, const Value& val) {
        return static_cast<std::uint32_t>(val.asInt());
    }
    static Value toValue(HostContext& ctx, std::uint32_t val) {
        return Value::Int(static_cast<std::int64_t>(val));
    }
};

template<>
struct TypeConverter<float> {
    static float fromValue(HostContext& ctx, const Value& val) {
        // Support implicit int->float conversion
        if (val.isInt()) {
            return static_cast<float>(val.asInt());
        }
        return static_cast<float>(val.asFloat());
    }
    static Value toValue(HostContext& ctx, float val) {
        return Value::Float(static_cast<double>(val));
    }
};

template<>
struct TypeConverter<double> {
    static double fromValue(HostContext& ctx, const Value& val) {
        // Support implicit int->double conversion
        if (val.isInt()) {
            return static_cast<double>(val.asInt());
        }
        return val.asFloat();
    }
    static Value toValue(HostContext& ctx, double val) {
        return Value::Float(val);
    }
};

template<>
struct TypeConverter<bool> {
    static bool fromValue(HostContext& ctx, const Value& val) {
        return val.asInt() != 0;
    }
    static Value toValue(HostContext& ctx, bool val) {
        return Value::Int(val ? 1 : 0);
    }
};

template<>
struct TypeConverter<std::string> {
    static std::string fromValue(HostContext& ctx, const Value& val) {
        return ctx.__str__(val);
    }
    static Value toValue(HostContext& ctx, const std::string& val) {
        return ctx.createString(val);
    }
};

template<>
struct TypeConverter<const char*> {
    static Value toValue(HostContext& ctx, const char* val) {
        return ctx.createString(val);
    }
};

template<>
struct TypeConverter<Value> {
    static Value fromValue(HostContext& ctx, const Value& val) {
        return val;
    }
    static Value toValue(HostContext& ctx, const Value& val) {
        return val;
    }
};

template<>
struct TypeConverter<void> {
    static Value toValue(HostContext& ctx) {
        return Value::Nil();
    }
};

// Helper for argument unpacking
template<typename... Args, std::size_t... Is>
std::tuple<Args...> unpackArgs(HostContext& ctx, const std::vector<Value>& args, std::index_sequence<Is...>) {
    if (args.size() != sizeof...(Args)) {
        throw std::runtime_error("Argument count mismatch");
    }
    return std::make_tuple(TypeConverter<Args>::fromValue(ctx, args[Is])...);
}

// Function wrapper
template<typename R, typename... Args>
struct FunctionWrapper {
    using FuncType = std::function<R(Args...)>;
    
    static Value invoke(HostContext& ctx, const std::vector<Value>& args, FuncType func) {
        auto argTuple = unpackArgs<Args...>(ctx, args, std::index_sequence_for<Args...>{});
        if constexpr (std::is_void_v<R>) {
            std::apply(func, argTuple);
            return TypeConverter<void>::toValue(ctx);
        } else {
            R result = std::apply(func, argTuple);
            return TypeConverter<R>::toValue(ctx, result);
        }
    }
};

// Method wrapper (with 'this' pointer)
template<typename T, typename R, typename... Args>
struct MethodWrapper {
    using MethodType = R (T::*)(Args...);
    using ConstMethodType = R (T::*)(Args...) const;
    
    static Value invoke(HostContext& ctx, Object& self, const std::vector<Value>& args, MethodType method);
    static Value invoke(HostContext& ctx, Object& self, const std::vector<Value>& args, ConstMethodType method);
};

} // namespace detail

// ============================================================================
// Native Object Wrapper (New V2 API)
// ============================================================================

template<typename T>
class NativeObjectWrapper : public Object {
public:
    NativeObjectWrapper(const Type& typeRef, T* ptr, bool ownsPtr = false)
        : type_(&typeRef), ptr_(ptr), ownsPtr_(ownsPtr) {}
    
    ~NativeObjectWrapper() override {
        if (ownsPtr_ && ptr_) {
            delete ptr_;
        }
    }
    
    const Type& getType() const override { return *type_; }
    T* getNativePtr() { return ptr_; }
    const T* getNativePtr() const { return ptr_; }
    
private:
    const Type* type_;
    T* ptr_;
    bool ownsPtr_;
};

// ============================================================================
// Class Binder (New V2 API)
// ============================================================================

class ClassBinder {
public:
    ClassBinder(BindingContext& ctx, const std::string& className);
    ~ClassBinder();  // Registers all bindings when the binder goes out of scope
    
    // Disable copy to ensure RAIIsemantics
    ClassBinder(const ClassBinder&) = delete;
    ClassBinder& operator=(const ClassBinder&) = delete;
    
    // Enable move with custom implementation
    ClassBinder(ClassBinder&& other) noexcept;
    ClassBinder& operator=(ClassBinder&&) = delete;
    
    // Bind constructor
    template<typename T, typename... Args>
    ClassBinder& constructor();
    
    // Bind method
    template<typename T, typename R, typename... Args>
    ClassBinder& method(const std::string& name, R (T::*method)(Args...));
    
    // Bind const method
    template<typename T, typename R, typename... Args>
    ClassBinder& method(const std::string& name, R (T::*method)(Args...) const);
    
    // Bind property getter/setter
    template<typename T, typename GetterRet, typename SetterArg = GetterRet>
    ClassBinder& property(const std::string& name, 
                         GetterRet (T::*getter)() const,
                         void (T::*setter)(SetterArg) = nullptr);
    
    // Bind field directly
    template<typename T, typename FieldType>
    ClassBinder& field(const std::string& name, FieldType T::*field);
    
    // Finalize and register all bindings
    void finalize();
    
private:
    friend class BindingContext;
    
    BindingContext& ctx_;
    std::string className_;
    Type* typePtr_ = nullptr;  // Non-const pointer to the Type object for registration
    std::function<Value(HostContext&, const std::vector<Value>&)> constructorFunc_;
    std::unordered_map<std::string, std::function<Value(HostContext&, Object&, const std::vector<Value>&)>> methods_;
    std::unordered_map<std::string, std::function<Value(HostContext&, Object&)>> propertyGetters_;
    std::unordered_map<std::string, std::function<Value(HostContext&, Object&, const Value&)>> propertySetters_;
};

// ============================================================================
// Binding Context (New V2 API - Main Interface)
// ============================================================================

class BindingContext {
public:
    explicit BindingContext(HostRegistry& registry);
    
    // Bind global function pointer
    template<typename R, typename... Args>
    void function(const std::string& name, R (*func)(Args...)) {
        using FuncType = std::function<R(Args...)>;
        registry_.bind(name, [func](HostContext& ctx, const std::vector<Value>& args) {
            return detail::FunctionWrapper<R, Args...>::invoke(ctx, args, FuncType(func));
        });
    }
    
    // Bind std::function
    template<typename R, typename... Args>
    void function(const std::string& name, std::function<R(Args...)> func) {
        registry_.bind(name, [func](HostContext& ctx, const std::vector<Value>& args) {
            return detail::FunctionWrapper<R, Args...>::invoke(ctx, args, func);
        });
    }
    
    // Bind lambda (with deduction)
    template<typename F>
    void function(const std::string& name, F&& func) {
        // Convert lambda to std::function to deduce signature
        using FuncType = decltype(std::function{std::forward<F>(func)});
        function(name, FuncType(std::forward<F>(func)));
    }
    
    // Begin class binding
    template<typename T>
    ClassBinder beginClass(const std::string& className);
    
    // Access to underlying registry (for backward compatibility)
    HostRegistry& registry() { return registry_; }
    
private:
    HostRegistry& registry_;
};

// ============================================================================
// Script Callable Invoker (C++ calls GS script)
// ============================================================================

class ScriptCallableInvoker {
public:
    ScriptCallableInvoker(HostContext& ctx, const Value& callable);
    
    // Call script function from C++
    template<typename R, typename... Args>
    R call(Args... args);
    
private:
    Value invokeInternal(const std::vector<Value>& args);
    
    HostContext& ctx_;
    Value callable_;
};

// ============================================================================
// Native Type Registry (for TypeConverter<T>::toValue)
// ============================================================================

void registerNativeType(const std::type_info& typeInfo, const Type& type);
const Type* getNativeType(const std::type_info& typeInfo);

} // namespace gs

// Include template implementations
#include "gs/binding.inl"
