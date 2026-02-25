# GameScript Binding API

## Overview

GameScript provides a modern, type-safe C++ binding system inspired by QuickJS. The binding API dramatically reduces boilerplate code while providing compile-time type safety.

## Quick Start

### Include the Header

```cpp
#include "gs/runtime.hpp"  // Includes binding.hpp automatically
```

### Bind Global Functions

```cpp
int Add(int a, int b) { return a + b; }

gs::Runtime runtime;
gs::BindingContext bindings(runtime.host());

bindings.function("Add", &Add);
```

### Bind Classes

```cpp
struct Vec2 {
    float x, y;
    float length() const { return std::sqrt(x*x + y*y); }
};

bindings.beginClass<Vec2>("Vec2")
    .constructor<float, float>()
    .field("x", &Vec2::x)
    .field("y", &Vec2::y)
    .method("length", &Vec2::length);
```

## Features

### ✅ Automatic Type Conversion
- Primitives: `int32`, `int64`, `uint32`, `float`, `double`, `bool`
- Strings: `std::string`, `const char*`
- Custom objects: via `NativeObjectWrapper<T>`
- Extensible: add your own `TypeConverter<T>` specializations

### ✅ Fluent Chainable API
```cpp
bindings.beginClass<Player>("Player")
    .constructor<std::string>()
    .property("name", &Player::getName, &Player::setName)
    .property("health", &Player::getHealth, &Player::setHealth)
    .method("takeDamage", &Player::takeDamage);
```

### ✅ Direct Field Access
```cpp
bindings.beginClass<Vec2>("Vec2")
    .field("x", &Vec2::x)  // No getters/setters needed!
    .field("y", &Vec2::y);
```

### ✅ Property Binding (Getter/Setter)
```cpp
bindings.beginClass<Actor>("Actor")
    .property("name", &Actor::getName, &Actor::setName)
    .property("health", &Actor::getHealth, &Actor::setHealth);
```

### ✅ Script Callbacks (Coming Soon)
```cpp
gs::ScriptCallableInvoker invoker(ctx, scriptFunction);
int result = invoker.call<int>(arg1, arg2);
```

## Comparison: Old vs New

### Before (Manual Binding - 120+ lines)

```cpp
class Vec2Object : public gs::Object {
    // ... 20 lines of boilerplate
};

class Vec2Type : public gs::Type {
public:
    Vec2Type() {
        registerMethodAttribute("length", 0, 
            [this](gs::Object& self, const std::vector<gs::Value>& args) {
                if (args.size() != 0) {
                    throw std::runtime_error("...");
                }
                auto& vecObj = require(self);
                float len = vecObj.value().length();
                return gs::Value::Int(static_cast<std::int64_t>(len));
            });
        // ... repeat 20 more times
    }
    // ... another 80 lines
};

host.bind("Vec2", [](gs::HostContext& ctx, ...) {
    // ... 20 lines of manual construction
});
```

### After (Modern Binding - 6 lines)

```cpp
bindings.beginClass<Vec2>("Vec2")
    .constructor<float, float>()
    .field("x", &Vec2::x)
    .field("y", &Vec2::y)
    .method("length", &Vec2::length);
```

**Result: 95% less code!**

## Type Conversion

### Built-in Converters

| C++ Type         | GameScript Type |
|------------------|-----------------|
| `int32`, `int64` | Int            |
| `uint32`         | Int            |
| `float`, `double`| Float          |
| `bool`           | Bool           |
| `std::string`    | String         |
| `const char*`    | String         |
| `T*`             | Object ref     |
| `Value`          | Pass-through   |

### Custom Type Converters

```cpp
namespace gs::detail {
template<>
struct TypeConverter<MyCustomType> {
    static MyCustomType fromValue(HostContext& ctx, const Value& val) {
        // Convert GS Value → C++ type
    }
    
    static Value toValue(HostContext& ctx, const MyCustomType& val) {
        // Convert C++ type → GS Value
    }
};
}
```

## UE5 Integration

### Binding UE5 Types

```cpp
// FVector
bindings.beginClass<FVector>("FVector")
    .constructor<float, float, float>()
    .field("X", &FVector::X)
    .field("Y", &FVector::Y)
    .field("Z", &FVector::Z)
    .method("Size", &FVector::Size)
    .method("GetSafeNormal", &FVector::GetSafeNormal);

// AActor
bindings.beginClass<AActor>("Actor")
    .method("GetActorLocation", &AActor::GetActorLocation)
    .method("SetActorLocation", &AActor::SetActorLocation)
    .method("Destroy", &AActor::Destroy);
```

### Blueprint Interop

```cpp
// GS can call Blueprint events
bindings.function("TriggerBlueprintEvent", 
    [](const std::string& eventName) {
        OnScriptEvent.Broadcast(FName(eventName.c_str()));
    });

// Blueprint can call GS functions
UFUNCTION(BlueprintCallable)
float CallScript(const FString& FuncName) {
    gs::Value func = runtime_.getGlobal(TCHAR_TO_UTF8(*FuncName));
    gs::ScriptCallableInvoker invoker(runtime_.context(), func);
    return invoker.call<float>();
}
```

## Advanced Usage

### Binding Lambdas

```cpp
bindings.function("GetSystemTime", []() -> std::int64_t {
    return std::chrono::system_clock::now().time_since_epoch().count();
});
```

### Variadic Arguments (Manual)

```cpp
bindings.registry().bind("LogMultiple", 
    [](gs::HostContext& ctx, const std::vector<gs::Value>& args) {
        for (const auto& arg : args) {
            std::cout << ctx.__str__(arg) << "\n";
        }
        return gs::Value::Nil();
    });
```

### Const and Non-const Methods

```cpp
class Container {
public:
    int size() const { return size_; }
    void add(int value) { /*...*/ }
};

bindings.beginClass<Container>("Container")
    .method("size", &Container::size)    // const method
    .method("add", &Container::add);     // non-const method
```

## Error Handling

All binding operations throw `std::runtime_error` on failure:
- Type mismatch
- Argument count mismatch
- Invalid object reference
- Null pointer access

## Performance

- **Zero-cost abstraction**: Templates expand at compile time
- **No runtime overhead**: Same performance as manual binding
- **Optimized conversions**: Inlined type conversions

## Migration from Old API

### Option 1: Incremental Migration

Keep old bindings working while adding new ones:

```cpp
// Old API (still works)
host.bind("OldFunction", [](gs::HostContext& ctx, ...) { });

// New API (recommended)
bindings.function("NewFunction", &MyFunction);
```

### Option 2: Complete Replacement

Replace all old-style bindings with new API:

1. Replace `host.bind()` with `bindings.function()`
2. Replace custom Type classes with `bindings.beginClass()`
3. Remove Object wrappers (use `NativeObjectWrapper<T>` automatically)

## Examples

See:
- `docs/examples/binding_comparison.cpp` - Full comparison example
- `app/main.cpp` - Production usage example
- `docs/BINDING_V2_GUIDE.md` - Comprehensive guide

## API Reference

### BindingContext

```cpp
class BindingContext {
    // Bind global function
    template<typename R, typename... Args>
    void function(const std::string& name, R (*func)(Args...));
    
    // Bind lambda
    template<typename R, typename... Args>
    void function(const std::string& name, std::function<R(Args...)> func);
    
    // Begin class binding
    template<typename T>
    ClassBinder beginClass(const std::string& className);
    
    // Access underlying registry
    HostRegistry& registry();
};
```

### ClassBinder

```cpp
class ClassBinder {
    // Bind constructor
    template<typename T, typename... Args>
    ClassBinder& constructor();
    
    // Bind method
    template<typename T, typename R, typename... Args>
    ClassBinder& method(const std::string& name, R (T::*method)(Args...));
    
    // Bind const method
    template<typename T, typename R, typename... Args>
    ClassBinder& method(const std::string& name, R (T::*method)(Args...) const);
    
    // Bind property (getter/setter)
    template<typename T, typename PropType>
    ClassBinder& property(const std::string& name, 
                         PropType (T::*getter)() const,
                         void (T::*setter)(PropType) = nullptr);
    
    // Bind field directly
    template<typename T, typename FieldType>
    ClassBinder& field(const std::string& name, FieldType T::*field);
};
```

## License

Part of the GameScript project.

---

**Version**: 2.0.0  
**Status**: Production Ready  
**Date**: 2026-02-25
