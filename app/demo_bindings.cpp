#include "demo_bindings.hpp"

#include "gs/bound_class_type.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace gs::demo {
namespace {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float inX, float inY) : x(inX), y(inY) {}

    float length() const {
        return std::sqrt(x * x + y * y);
    }

    Vec2 normalize() const {
        const float len = length();
        if (len > 0.0f) {
            return Vec2(x / len, y / len);
        }
        return Vec2(0.0f, 0.0f);
    }

    Vec2 add(const Vec2& other) const {
        return Vec2(x + other.x, y + other.y);
    }
};

class Entity {
public:
    std::uint32_t mHP = 100;
    std::uint32_t mMP = 50;
    float mSpeed = 5.0f;
    Vec2 mPosition;

    Entity() = default;

    void GotoPoint(const Vec2& p) {
        mPosition = p;
    }

    int GetDataSize() const {
        std::vector<int> data;
        return static_cast<int>(data.size());
    }

    Vec2 getPosition() const { return mPosition; }
    void setPosition(const Vec2& pos) { mPosition = pos; }
};

} // namespace

void bindDefaultDemoExports(HostRegistry& host) {
    static BoundClassType vec2Type("Vec2");
    static BoundClassType entityType("Entity");

    registerNativeType(typeid(Vec2), vec2Type);
    registerNativeType(typeid(Entity), entityType);

    BindingContext bindings(host);

    {
        auto binder = bindings.beginClass<Vec2>("Vec2");
        binder.constructor<Vec2, float, float>();
        binder.field<Vec2>("x", &Vec2::x);
        binder.field<Vec2>("y", &Vec2::y);
        binder.method<Vec2>("length", &Vec2::length);
        binder.method<Vec2>("normalize", &Vec2::normalize);
        binder.method<Vec2>("add", &Vec2::add);
        binder.finalize();
    }

    {
        auto binder = bindings.beginClass<Entity>("Entity");
        binder.constructor<Entity>();
        binder.field<Entity>("HP", &Entity::mHP);
        binder.field<Entity>("MP", &Entity::mMP);
        binder.field<Entity>("Speed", &Entity::mSpeed);
        binder.property<Entity>("Position", &Entity::getPosition, &Entity::setPosition);
        binder.method<Entity>("GotoPoint", &Entity::GotoPoint);
        binder.method<Entity>("GetDataSize", &Entity::GetDataSize);
        binder.finalize();
    }

    bindings.function("Distance", [](const Vec2& a, const Vec2& b) -> float {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    });
}

} // namespace gs::demo
