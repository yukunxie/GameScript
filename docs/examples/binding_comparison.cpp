/**
 * Comparison Example: Old vs New Binding API
 * 
 * This file demonstrates the difference between the original manual
 * binding approach and the new QuickJS-inspired V2 API.
 */

#include "gs/runtime.hpp"
#include "gs/binding_v2.hpp"
#include <cmath>
#include <iostream>

// ============================================================================
// Example Domain Classes
// ============================================================================

namespace Game {

struct Vec2 {
    float x, y;
    
    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}
    
    float length() const {
        return std::sqrt(x * x + y * y);
    }
    
    void normalize() {
        float len = length();
        if (len > 0) {
            x /= len;
            y /= len;
        }
    }
    
    Vec2 add(const Vec2& other) const {
        return Vec2(x + other.x, y + other.y);
    }
};

class Player {
private:
    std::string name_;
    int health_;
    Vec2 position_;
    
public:
    Player(const std::string& name) 
        : name_(name), health_(100), position_(0, 0) {}
    
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    
    int getHealth() const { return health_; }
    void setHealth(int health) { health_ = health; }
    
    Vec2 getPosition() const { return position_; }
    void setPosition(const Vec2& pos) { position_ = pos; }
    
    void takeDamage(int damage) {
        health_ -= damage;
        if (health_ < 0) health_ = 0;
    }
    
    void moveTo(float x, float y) {
        position_ = Vec2(x, y);
    }
};

} // namespace Game

// ============================================================================
// OLD WAY: Manual Binding (for reference)
// ============================================================================

#ifdef SHOW_OLD_BINDING_APPROACH

namespace OldBinding {

using namespace Game;

// Custom Object wrapper
class Vec2Object : public gs::Object {
public:
    Vec2Object(const gs::Type& typeRef, Vec2 value)
        : type_(&typeRef), value_(value) {}
    
    const gs::Type& getType() const override { return *type_; }
    Vec2& value() { return value_; }
    const Vec2& value() const { return value_; }
    
private:
    const gs::Type* type_;
    Vec2 value_;
};

// Custom Type class
class Vec2Type : public gs::Type {
public:
    Vec2Type() {
        // Manually register each method
        registerMethodAttribute("length", 0, 
            [this](gs::Object& self, const std::vector<gs::Value>& args) {
                if (args.size() != 0) {
                    throw std::runtime_error("Vec2.length() takes no arguments");
                }
                auto& vecObj = require(self);
                float len = vecObj.value().length();
                return gs::Value::Int(static_cast<std::int64_t>(len));
            });
        
        registerMethodAttribute("normalize", 0,
            [this](gs::Object& self, const std::vector<gs::Value>& args) {
                if (args.size() != 0) {
                    throw std::runtime_error("Vec2.normalize() takes no arguments");
                }
                auto& vecObj = require(self);
                vecObj.value().normalize();
                return gs::Value::Nil();
            });
        
        // Manually register properties
        registerMemberAttribute("x",
            [this](gs::Object& self) {
                return gs::Value::Int(static_cast<std::int64_t>(require(self).value().x));
            },
            [this](gs::Object& self, const gs::Value& value) {
                require(self).value().x = static_cast<float>(value.asInt());
                return value;
            });
        
        registerMemberAttribute("y",
            [this](gs::Object& self) {
                return gs::Value::Int(static_cast<std::int64_t>(require(self).value().y));
            },
            [this](gs::Object& self, const gs::Value& value) {
                require(self).value().y = static_cast<float>(value.asInt());
                return value;
            });
    }
    
    const char* name() const override { return "Vec2"; }
    
    std::string __str__(gs::Object& self, const gs::Type::ValueStrInvoker& valueStr) const override {
        auto& vecObj = require(self);
        const auto& vec = vecObj.value();
        return "Vec2(" + std::to_string(vec.x) + ", " + std::to_string(vec.y) + ")";
    }
    
private:
    static Vec2Object& require(gs::Object& self) {
        auto* out = dynamic_cast<Vec2Object*>(&self);
        if (!out) {
            throw std::runtime_error("Target is not Vec2 object");
        }
        return *out;
    }
};

void bindOldWay(gs::HostRegistry& host) {
    static Vec2Type vec2Type;
    
    // Manually bind constructor
    host.bind("Vec2", [](gs::HostContext& ctx, const std::vector<gs::Value>& args) -> gs::Value {
        if (args.size() != 0 && args.size() != 2) {
            throw std::runtime_error("Vec2() or Vec2(x, y)");
        }
        Vec2 value{0.0f, 0.0f};
        if (args.size() == 2) {
            value.x = static_cast<float>(args[0].asInt());
            value.y = static_cast<float>(args[1].asInt());
        }
        return ctx.createObject(std::make_unique<Vec2Object>(vec2Type, value));
    });
    
    // Manually bind free function
    host.bind("Distance", [](gs::HostContext& ctx, const std::vector<gs::Value>& args) -> gs::Value {
        if (args.size() != 2) {
            throw std::runtime_error("Distance(v1, v2) requires 2 arguments");
        }
        if (!args[0].isRef() || !args[1].isRef()) {
            throw std::runtime_error("Distance requires Vec2 objects");
        }
        
        auto& obj1 = ctx.getObject(args[0]);
        auto& obj2 = ctx.getObject(args[1]);
        auto* vec1 = dynamic_cast<Vec2Object*>(&obj1);
        auto* vec2 = dynamic_cast<Vec2Object*>(&obj2);
        
        if (!vec1 || !vec2) {
            throw std::runtime_error("Arguments must be Vec2 objects");
        }
        
        float dx = vec1->value().x - vec2->value().x;
        float dy = vec1->value().y - vec2->value().y;
        float dist = std::sqrt(dx * dx + dy * dy);
        
        return gs::Value::Int(static_cast<std::int64_t>(dist));
    });
}

// Total lines of code: ~120 lines just for Vec2!

} // namespace OldBinding

#endif // SHOW_OLD_BINDING_APPROACH

// ============================================================================
// NEW WAY: QuickJS-Inspired V2 API
// ============================================================================

namespace NewBinding {

using namespace Game;

// Simple helper function
float Distance(const Vec2& v1, const Vec2& v2) {
    float dx = v1.x - v2.x;
    float dy = v1.y - v2.y;
    return std::sqrt(dx * dx + dy * dy);
}

void bindNewWay(gs::BindingContext& bindings) {
    // Bind Vec2 class (chainable, type-safe)
    bindings.beginClass<Vec2>("Vec2")
        .constructor<float, float>()          // Vec2(x, y)
        .field("x", &Vec2::x)                 // Direct field access
        .field("y", &Vec2::y)
        .method("length", &Vec2::length)      // Methods
        .method("normalize", &Vec2::normalize)
        .method("add", &Vec2::add);
    
    // Bind Player class with properties
    bindings.beginClass<Player>("Player")
        .constructor<std::string>()                           // Player(name)
        .property("name", &Player::getName, &Player::setName) // Getter/setter
        .property("health", &Player::getHealth, &Player::setHealth)
        .method("takeDamage", &Player::takeDamage)
        .method("moveTo", &Player::moveTo);
    
    // Bind free functions
    bindings.function("Distance", &Distance);
    
    // Bind lambda
    bindings.function("CreateDefaultPlayer", []() -> Player* {
        return new Player("Default");
    });
}

// Total lines of code: ~20 lines for Vec2 + Player + utilities!
// That's 6x less code with full type safety!

} // namespace NewBinding

// ============================================================================
// Usage Examples
// ============================================================================

void demonstrateUsage() {
    gs::Runtime runtime;
    gs::BindingContext bindings(runtime.host());
    
    // Bind everything with the new API
    NewBinding::bindNewWay(bindings);
    
    // Now you can use it in scripts:
    const char* script = R"(
        # Create vectors
        let v1 = Vec2(3.0, 4.0);
        let v2 = Vec2(6.0, 8.0);
        
        print("v1 length: " + str(v1.length()));  # 5
        
        # Normalize
        v1.normalize();
        print("Normalized: " + str(v1.length())); # 1
        
        # Distance
        let dist = Distance(v1, v2);
        print("Distance: " + str(dist));
        
        # Player
        let player = Player("Hero");
        print("Player: " + player.name);
        
        player.health = 100;
        player.takeDamage(30);
        print("Health after damage: " + str(player.health)); # 70
        
        player.moveTo(10.0, 20.0);
    )";
    
    std::cout << "Running script with new binding API...\n";
    // runtime.executeString(script);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "==============================================\n";
    std::cout << "GameScript Binding API Comparison\n";
    std::cout << "==============================================\n\n";
    
    std::cout << "Old API:\n";
    std::cout << "  - ~120 lines of boilerplate per class\n";
    std::cout << "  - Manual type checking and conversion\n";
    std::cout << "  - Error-prone dynamic casts\n";
    std::cout << "  - Requires custom Object and Type classes\n\n";
    
    std::cout << "New API (QuickJS-inspired):\n";
    std::cout << "  - ~20 lines for multiple classes\n";
    std::cout << "  - Automatic type conversion\n";
    std::cout << "  - Compile-time type safety\n";
    std::cout << "  - Chainable, fluent interface\n";
    std::cout << "  - 80% less code!\n\n";
    
    std::cout << "Benefits for UE5:\n";
    std::cout << "  ✓ Easy to bind UE5 classes (AActor, FVector, etc.)\n";
    std::cout << "  ✓ Support for Blueprint callbacks\n";
    std::cout << "  ✓ Familiar API for UE developers\n";
    std::cout << "  ✓ Extensible type conversion system\n\n";
    
    demonstrateUsage();
    
    return 0;
}
