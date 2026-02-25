# GameScript V2 Binding System

## Overview

The V2 binding system is inspired by QuickJS's elegant approach to binding native C++ APIs. It provides:

- **Type-safe automatic conversion** between C++ and GameScript types
- **Zero-boilerplate** function and method binding
- **Chainable API** for class registration
- **Script callback** support (C++ calling GS functions)
- **UE5-friendly** design for easy integration

## Quick Start

### 1. Binding Global Functions

```cpp
#include "gs/binding_v2.hpp"

// Example functions
int Add(int a, int b) {
    return a + b;
}

std::string Greet(const std::string& name) {
    return "Hello, " + name + "!";
}

void LogMessage(const std::string& msg) {
    std::cout << msg << std::endl;
}

// Bind them
gs::Runtime runtime;
gs::BindingContext bindings(runtime.host());

bindings.function("Add", &Add);
bindings.function("Greet", &Greet);
bindings.function("LogMessage", &LogMessage);
```

In GameScript:
```js
let sum = Add(10, 20);        # -> 30
let msg = Greet("World");     # -> "Hello, World!"
LogMessage("Test message");
```

### 2. Binding Classes

```cpp
// UE5-style class
class FVector {
public:
    float X, Y, Z;
    
    FVector() : X(0), Y(0), Z(0) {}
    FVector(float x, float y, float z) : X(x), Y(y), Z(z) {}
    
    float Length() const {
        return std::sqrt(X*X + Y*Y + Z*Z);
    }
    
    void Normalize() {
        float len = Length();
        if (len > 0) {
            X /= len;
            Y /= len;
            Z /= len;
        }
    }
    
    FVector Add(const FVector& other) const {
        return FVector(X + other.X, Y + other.Y, Z + other.Z);
    }
};

// Bind the class
bindings.beginClass<FVector>("FVector")
    .constructor<float, float, float>()  // FVector(x, y, z)
    .field("X", &FVector::X)
    .field("Y", &FVector::Y)
    .field("Z", &FVector::Z)
    .method("Length", &FVector::Length)
    .method("Normalize", &FVector::Normalize)
    .method("Add", &FVector::Add);
```

In GameScript:
```js
let v = FVector(1.0, 2.0, 3.0);
print(v.X);              # -> 1
print(v.Length());       # -> 3.74...

v.Normalize();
print(v.Length());       # -> 1.0

let v2 = FVector(10, 20, 30);
let result = v.Add(v2);
```

### 3. Property Binding (Getter/Setter)

```cpp
class AActor {
private:
    std::string name_;
    bool isActive_;
    
public:
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }
    
    bool IsActive() const { return isActive_; }
    void SetActive(bool active) { isActive_ = active; }
};

bindings.beginClass<AActor>("Actor")
    .property("Name", &AActor::GetName, &AActor::SetName)
    .property("IsActive", &AActor::IsActive, &AActor::SetActive);
```

In GameScript:
```js
let actor = Actor();
actor.Name = "Player";
actor.IsActive = true;

print(actor.Name);       # -> "Player"
print(actor.IsActive);   # -> true
```

### 4. Calling GameScript Functions from C++

```cpp
// In GameScript (my_script.gs):
fn OnPlayerSpawn(playerName, position) {
    print("Player spawned: " + playerName);
    print("Position: " + str(position));
    return true;
}

// In C++:
#include "gs/binding_v2.hpp"

gs::Runtime runtime;
runtime.loadSourceFile("my_script.gs");

// Get the function
gs::Value callback = runtime.getGlobal("OnPlayerSpawn");

// Create invoker
gs::ScriptCallableInvoker invoker(runtime.context(), callback);

// Call with type-safe arguments
FVector spawnPos(100, 200, 50);
bool result = invoker.call<bool>(std::string("Alice"), spawnPos);
```

### 5. Script Callbacks (C++ stores and calls GS functions)

```cpp
// C++ class that accepts callbacks
class EventSystem {
public:
    using CallbackType = std::function<void(const std::string&)>;
    
    void RegisterCallback(CallbackType callback) {
        callback_ = callback;
    }
    
    void TriggerEvent(const std::string& eventName) {
        if (callback_) {
            callback_(eventName);
        }
    }
    
private:
    CallbackType callback_;
};

// Bind it
bindings.beginClass<EventSystem>("EventSystem")
    .method("RegisterCallback", &EventSystem::RegisterCallback)
    .method("TriggerEvent", &EventSystem::TriggerEvent);
```

In GameScript:
```js
let events = EventSystem();

events.RegisterCallback(fn(eventName) {
    print("Event triggered: " + eventName);
});

events.TriggerEvent("OnPlayerJoin");  # -> "Event triggered: OnPlayerJoin"
```

## UE5 Integration Example

### Binding UE5 Actors

```cpp
// In your UE5 module
class UGameScriptSubsystem : public UGameInstanceSubsystem {
public:
    void InitializeBindings() {
        gs::BindingContext bindings(scriptRuntime_.host());
        
        // Bind FVector
        bindings.beginClass<FVector>("FVector")
            .constructor<float, float, float>()
            .field("X", &FVector::X)
            .field("Y", &FVector::Y)
            .field("Z", &FVector::Z)
            .method("Size", &FVector::Size)
            .method("GetSafeNormal", &FVector::GetSafeNormal);
        
        // Bind AActor (use a wrapper)
        bindings.beginClass<AActor>("Actor")
            .method("GetActorLocation", &AActor::GetActorLocation)
            .method("SetActorLocation", &AActor::SetActorLocation)
            .method("GetActorRotation", &AActor::GetActorRotation)
            .method("Destroy", &AActor::Destroy);
        
        // Bind global functions
        bindings.function("SpawnActor", [this](const std::string& className, FVector location) {
            // Spawn logic
            return SpawnActorInternal(className, location);
        });
        
        bindings.function("FindActorByName", [this](const std::string& name) {
            return FindActorInternal(name);
        });
    }
    
private:
    gs::Runtime scriptRuntime_;
};
```

### Blueprint-Script Interop

```cpp
// Allow Blueprints to call scripts
UFUNCTION(BlueprintCallable, Category="GameScript")
float CallScriptFunction(const FString& FunctionName, const TArray<float>& Args) {
    gs::Value func = scriptRuntime_.getGlobal(TCHAR_TO_UTF8(*FunctionName));
    gs::ScriptCallableInvoker invoker(scriptRuntime_.context(), func);
    
    // Convert arguments
    std::vector<float> args;
    for (float arg : Args) {
        args.push_back(arg);
    }
    
    return invoker.call<float>(args);
}

// Allow scripts to call Blueprints
void BindBlueprintCallbacks() {
    bindings.function("CallBlueprint", [this](const std::string& eventName) {
        // Trigger Blueprint event
        OnScriptEvent.Broadcast(FName(eventName.c_str()));
    });
}
```

## Type Conversion Table

| C++ Type         | GameScript Type | Notes                          |
|------------------|-----------------|--------------------------------|
| `int32`, `int64` | `Int`          | Automatic conversion           |
| `uint32`         | `Int`          | Converted to signed            |
| `float`          | `Float`        | Automatic conversion           |
| `double`         | `Float`        | -                              |
| `bool`           | `Bool`         | -                              |
| `std::string`    | `String`       | -                              |
| `const char*`    | `String`       | One-way (C++ → GS only)        |
| `T*`             | Object ref     | Custom objects via wrapper     |
| `Value`          | Any            | Pass-through                   |
| `void`           | `Nil`          | Functions returning void       |

## Advanced Features

### Custom Type Converters

```cpp
// Define conversion for FRotator
namespace gs::detail {
template<>
struct TypeConverter<FRotator> {
    static FRotator fromValue(HostContext& ctx, const Value& val) {
        // Extract from GS object
        auto& obj = ctx.getObject(val);
        // ... custom logic
    }
    
    static Value toValue(HostContext& ctx, const FRotator& rot) {
        // Wrap in GS object
        // ... custom logic
    }
};
}
```

### Variadic Arguments

```cpp
void LogMultiple(const std::vector<std::string>& messages) {
    for (const auto& msg : messages) {
        UE_LOG(LogTemp, Log, TEXT("%s"), *FString(msg.c_str()));
    }
}

// Bind with manual unpacking
bindings.function("LogMultiple", [](HostContext& ctx, const std::vector<Value>& args) {
    std::vector<std::string> messages;
    for (const auto& arg : args) {
        messages.push_back(ctx.__str__(arg));
    }
    LogMultiple(messages);
    return Value::Nil();
});
```

## Comparison with Old API

### Old Way (Manual)

```cpp
host.bind("Vec2", [this](gs::HostContext& ctx, const std::vector<gs::Value>& args) {
    if (args.size() != 0 && args.size() != 2) {
        throw std::runtime_error("Vec2() or Vec2(x, y)");
    }
    Vec2 value{0.0f, 0.0f};
    if (args.size() == 2) {
        value.x = static_cast<float>(args[0].asInt());
        value.y = static_cast<float>(args[1].asInt());
    }
    return ctx.createObject(std::make_unique<Vec2Object>(vec2Type_, value));
});

class Vec2Type : public gs::Type {
    // 50+ lines of boilerplate...
    GS_EXPORT_METHOD("length", 0, methodLength);
    GS_EXPORT_NUM_MEMBER("x", require(self).value().x, float);
    // ...
};
```

### New Way (QuickJS-inspired)

```cpp
bindings.beginClass<Vec2>("Vec2")
    .constructor<float, float>()
    .field("x", &Vec2::x)
    .field("y", &Vec2::y)
    .method("length", &Vec2::Length);
```

**Result: 80% less code, 100% type-safe!**

## Performance Notes

- Type conversions are inlined and optimized
- No additional runtime overhead compared to manual binding
- Zero-cost abstraction for most use cases
- Template meta-programming happens at compile time

## Next Steps

1. ✅ Core binding system implemented
2. ⏳ VM integration for script callbacks
3. ⏳ Extended type converters (TArray, TMap, etc.)
4. ⏳ UE5 reflection system auto-binding
5. ⏳ Blueprint integration helpers

## License

Part of the GameScript project.
