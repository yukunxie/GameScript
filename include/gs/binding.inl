// Template implementations for binding.hpp
// This file is automatically included by binding.hpp

#pragma once

namespace gs {

namespace detail {

// ============================================================================
// TypeConverter for custom pointers
// ============================================================================

template<typename T>
struct TypeConverter<T*> {
    static T* fromValue(HostContext& ctx, const Value& val) {
        if (!val.isRef()) {
            throw std::runtime_error("Expected object reference");
        }
        auto& obj = ctx.getObject(val);
        auto* wrapper = dynamic_cast<NativeObjectWrapper<T>*>(&obj);
        if (!wrapper) {
            throw std::runtime_error("Type mismatch in object conversion");
        }
        return wrapper->getNativePtr();
    }
    
    // Note: toValue needs access to Type, implemented in binding.cpp
    static Value toValue(HostContext& ctx, T* val);
};

// Generic TypeConverter for user-defined value types
// Enables pass-by-value for bound C++ classes
template<typename T>
    requires std::is_class_v<T> && (!std::is_same_v<T, std::string>)
struct TypeConverter<T> {
    static T fromValue(HostContext& ctx, const Value& val) {
        if (!val.isRef()) {
            throw std::runtime_error("Expected object reference");
        }
        auto& obj = ctx.getObject(val);
        auto* wrapper = dynamic_cast<NativeObjectWrapper<T>*>(&obj);
        if (!wrapper) {
            throw std::runtime_error("Type mismatch in object conversion");
        }
        // Return a copy of the wrapped object
        return *wrapper->getNativePtr();
    }
    
    static Value toValue(HostContext& ctx, const T& val) {
        // Look up the registered Type for T
        const Type* typePtr = getNativeType(typeid(T));
        if (!typePtr) {
            throw std::runtime_error(std::string("Type not registered: ") + typeid(T).name());
        }
        
        // Create a heap copy and wrap it
        T* heapCopy = new T(val);
        auto wrapper = std::make_unique<NativeObjectWrapper<T>>(*typePtr, heapCopy, true);
        return ctx.createObject(std::move(wrapper));
    }
};

// TypeConverter for const references (avoid unnecessary copies)
template<typename T>
    requires std::is_class_v<T> && (!std::is_same_v<T, std::string>)
struct TypeConverter<const T&> {
    static const T& fromValue(HostContext& ctx, const Value& val) {
        if (!val.isRef()) {
            throw std::runtime_error("Expected object reference");
        }
        auto& obj = ctx.getObject(val);
        auto* wrapper = dynamic_cast<NativeObjectWrapper<T>*>(&obj);
        if (!wrapper) {
            throw std::runtime_error("Type mismatch in object conversion");
        }
        return *wrapper->getNativePtr();
    }
    
    static Value toValue(HostContext& ctx, const T& val) {
        return TypeConverter<T>::toValue(ctx, val);
    }
};

// ============================================================================
// MethodWrapper implementations
// ============================================================================

template<typename T, typename R, typename... Args>
Value detail::MethodWrapper<T, R, Args...>::invoke(HostContext& ctx, Object& self,
                                          const std::vector<Value>& args, MethodType method) {
    auto* wrapper = dynamic_cast<NativeObjectWrapper<T>*>(&self);
    if (!wrapper) {
        throw std::runtime_error("Invalid this pointer");
    }
    T* instance = wrapper->getNativePtr();
    
    auto argTuple = unpackArgs<Args...>(ctx, args, std::index_sequence_for<Args...>{});
    if constexpr (std::is_void_v<R>) {
        std::apply([instance, method](Args... args) {
            (instance->*method)(args...);
        }, argTuple);
        return TypeConverter<void>::toValue(ctx);
    } else {
        R result = std::apply([instance, method](Args... args) {
            return (instance->*method)(args...);
        }, argTuple);
        return TypeConverter<R>::toValue(ctx, result);
    }
}

template<typename T, typename R, typename... Args>
Value detail::MethodWrapper<T, R, Args...>::invoke(HostContext& ctx, Object& self, 
                                          const std::vector<Value>& args, ConstMethodType method) {
    auto* wrapper = dynamic_cast<NativeObjectWrapper<T>*>(&self);
    if (!wrapper) {
        throw std::runtime_error("Invalid this pointer");
    }
    const T* instance = wrapper->getNativePtr();
    
    auto argTuple = unpackArgs<Args...>(ctx, args, std::index_sequence_for<Args...>{});
    if constexpr (std::is_void_v<R>) {
        std::apply([instance, method](Args... args) {
            (instance->*method)(args...);
        }, argTuple);
        return TypeConverter<void>::toValue(ctx);
    } else {
        R result = std::apply([instance, method](Args... args) {
            return (instance->*method)(args...);
        }, argTuple);
        return TypeConverter<R>::toValue(ctx, result);
    }
}

} // namespace detail

// ============================================================================
// ClassBinder template implementations
// ============================================================================

template<typename T, typename... Args>
ClassBinder& ClassBinder::constructor() {
    constructorFunc_ = [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        auto argTuple = detail::unpackArgs<Args...>(ctx, args, std::index_sequence_for<Args...>{});
        T* instance = std::apply([](Args... args) {
            return new T(args...);
        }, argTuple);
        
        // Wrap the instance in a NativeObjectWrapper
        const Type* typePtr = getNativeType(typeid(T));
        if (!typePtr) {
            throw std::runtime_error(std::string("Type not registered: ") + typeid(T).name());
        }
        
        auto wrapper = std::make_unique<NativeObjectWrapper<T>>(*typePtr, instance, true);
        return ctx.createObject(std::move(wrapper));
    };
    return *this;
}

template<typename T, typename R, typename... Args>
ClassBinder& ClassBinder::method(const std::string& name, R (T::*method)(Args...)) {
    methods_[name] = [method](HostContext& ctx, Object& self, const std::vector<Value>& args) {
        return detail::MethodWrapper<T, R, Args...>::invoke(ctx, self, args, method);
    };
    return *this;
}

template<typename T, typename R, typename... Args>
ClassBinder& ClassBinder::method(const std::string& name, R (T::*method)(Args...) const) {
    methods_[name] = [method](HostContext& ctx, Object& self, const std::vector<Value>& args) {
        return detail::MethodWrapper<T, R, Args...>::invoke(ctx, self, args, method);
    };
    return *this;
}

template<typename T, typename GetterRet, typename SetterArg>
ClassBinder& ClassBinder::property(const std::string& name, 
                                   GetterRet (T::*getter)() const,
                                   void (T::*setter)(SetterArg)) {
    using GetterRetDecay = std::decay_t<GetterRet>;
    using SetterArgDecay = std::decay_t<SetterArg>;
    
    propertyGetters_[name] = [getter](HostContext& ctx, Object& self) {
        auto* wrapper = dynamic_cast<NativeObjectWrapper<T>*>(&self);
        if (!wrapper) {
            throw std::runtime_error("Invalid this pointer");
        }
        const T* instance = wrapper->getNativePtr();
        GetterRet value = (instance->*getter)();
        return detail::TypeConverter<GetterRetDecay>::toValue(ctx, value);
    };
    
    if (setter) {
        propertySetters_[name] = [setter](HostContext& ctx, Object& self, const Value& value) {
            auto* wrapper = dynamic_cast<NativeObjectWrapper<T>*>(&self);
            if (!wrapper) {
                throw std::runtime_error("Invalid this pointer");
            }
            T* instance = wrapper->getNativePtr();
            SetterArgDecay converted = detail::TypeConverter<SetterArgDecay>::fromValue(ctx, value);
            (instance->*setter)(converted);
            return value;
        };
    }
    
    return *this;
}

template<typename T, typename FieldType>
ClassBinder& ClassBinder::field(const std::string& name, FieldType T::*field) {
    propertyGetters_[name] = [field](HostContext& ctx, Object& self) {
        auto* wrapper = dynamic_cast<NativeObjectWrapper<T>*>(&self);
        if (!wrapper) {
            throw std::runtime_error("Invalid this pointer");
        }
        const T* instance = wrapper->getNativePtr();
        return detail::TypeConverter<FieldType>::toValue(ctx, instance->*field);
    };
    
    propertySetters_[name] = [field](HostContext& ctx, Object& self, const Value& value) {
        auto* wrapper = dynamic_cast<NativeObjectWrapper<T>*>(&self);
        if (!wrapper) {
            throw std::runtime_error("Invalid this pointer");
        }
        T* instance = wrapper->getNativePtr();
        instance->*field = detail::TypeConverter<FieldType>::fromValue(ctx, value);
        return value;
    };
    
    return *this;
}

// ============================================================================
// BindingContext template implementations
// ============================================================================

template<typename T>
ClassBinder BindingContext::beginClass(const std::string& className) {
    ClassBinder binder(*this, className);
    
    // Get the Type pointer for this class (should have been registered before)
    Type* typePtr = const_cast<Type*>(getNativeType(typeid(T)));
    if (!typePtr) {
        throw std::runtime_error("Type not registered before beginClass: " + className);
    }
    
    binder.typePtr_ = typePtr;
    return binder;
}

// ============================================================================
// ScriptCallableInvoker template implementations
// ============================================================================

template<typename R, typename... Args>
R ScriptCallableInvoker::call(Args... args) {
    std::vector<Value> argValues;
    (argValues.push_back(detail::TypeConverter<Args>::toValue(ctx_, args)), ...);
    
    Value result = invokeInternal(argValues);
    
    if constexpr (!std::is_void_v<R>) {
        return detail::TypeConverter<R>::fromValue(ctx_, result);
    }
}

} // namespace gs
