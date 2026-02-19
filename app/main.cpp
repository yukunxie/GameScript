#include "gs/runtime.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace MyGame
{
    struct Vec2
    {
        float x;
        float y;
    };

    class Entity
    {
    public:
       std::string mName;
       std::uint32_t mHP;
       std::uint32_t mMP;
       float mSpeed;
       Vec2 mPosition;

    public:
        void GotoPoint(const Vec2& p)
        {
            // todo
        }

        std::vector<int> GetData()
        {
            std::vector<int> data;
            // todo;
            return data;
        }
    };
    class Vec2Type;

    class Vec2Object : public gs::Object
    {
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

    class EntityObject : public gs::Object
    {
    public:
        EntityObject(const gs::Type& typeRef, Entity value)
            : type_(&typeRef), value_(std::move(value)) {}

        const gs::Type& getType() const override { return *type_; }
        Entity& value() { return value_; }
        const Entity& value() const { return value_; }

    private:
        const gs::Type* type_;
        Entity value_;
    };

    class Vec2Type : public gs::Type
    {
    public:
        const char* name() const override { return "Vec2"; }

        gs::Value callMethod(gs::Object& self,
                             const std::string& method,
                             const std::vector<gs::Value>& args) const override
        {
            auto& vec = require(self).value();
            if (method == "length") {
                if (!args.empty()) {
                    throw std::runtime_error("Vec2.length() requires 0 args");
                }
                const auto len = std::sqrt(static_cast<double>(vec.x * vec.x + vec.y * vec.y));
                return gs::Value::Int(static_cast<std::int64_t>(len));
            }
            throw std::runtime_error("Unknown Vec2 method: " + method);
        }

        gs::Value getMember(gs::Object& self, const std::string& member) const override
        {
            const auto& vec = require(self).value();
            if (member == "x") return gs::Value::Int(static_cast<std::int64_t>(vec.x));
            if (member == "y") return gs::Value::Int(static_cast<std::int64_t>(vec.y));
            throw std::runtime_error("Unknown Vec2 member: " + member);
        }

        gs::Value setMember(gs::Object& self, const std::string& member, const gs::Value& value) const override
        {
            auto& vec = require(self).value();
            if (member == "x") {
                vec.x = static_cast<float>(value.asInt());
                return value;
            }
            if (member == "y") {
                vec.y = static_cast<float>(value.asInt());
                return value;
            }
            throw std::runtime_error("Unknown Vec2 member: " + member);
        }

    private:
        static Vec2Object& require(gs::Object& self)
        {
            auto* out = dynamic_cast<Vec2Object*>(&self);
            if (!out) {
                throw std::runtime_error("Target is not Vec2 object");
            }
            return *out;
        }
    };

    class EntityType : public gs::Type
    {
    public:
        explicit EntityType(const Vec2Type& vec2Type)
            : vec2Type_(&vec2Type) {}

        const char* name() const override { return "Entity"; }

        gs::Value callMethod(gs::Object& self,
                             const std::string& method,
                             const std::vector<gs::Value>& args) const override
        {
            auto& entity = require(self).value();
            if (method == "goto_point") {
                if (args.size() != 1 || !args[0].isRef()) {
                    throw std::runtime_error("Entity.goto_point(Vec2) requires 1 Vec2 object arg");
                }
                throw std::runtime_error("Entity.goto_point requires object member assignment via position property");
            }
            if (method == "get_data") {
                if (!args.empty()) {
                    throw std::runtime_error("Entity.get_data() requires 0 args");
                }
                return gs::Value::Int(static_cast<std::int64_t>(entity.GetData().size()));
            }
            throw std::runtime_error("Unknown Entity method: " + method);
        }

        gs::Value getMember(gs::Object& self, const std::string& member) const override
        {
            const auto& e = require(self).value();
            if (member == "hp") return gs::Value::Int(e.mHP);
            if (member == "mp") return gs::Value::Int(e.mMP);
            if (member == "speed") return gs::Value::Int(static_cast<std::int64_t>(e.mSpeed));
            if (member == "x") return gs::Value::Int(static_cast<std::int64_t>(e.mPosition.x));
            if (member == "y") return gs::Value::Int(static_cast<std::int64_t>(e.mPosition.y));
            throw std::runtime_error("Unknown Entity member: " + member);
        }

        gs::Value setMember(gs::Object& self, const std::string& member, const gs::Value& value) const override
        {
            auto& e = require(self).value();
            if (member == "hp") {
                e.mHP = static_cast<std::uint32_t>(value.asInt());
                return value;
            }
            if (member == "mp") {
                e.mMP = static_cast<std::uint32_t>(value.asInt());
                return value;
            }
            if (member == "speed") {
                e.mSpeed = static_cast<float>(value.asInt());
                return value;
            }
            if (member == "x") {
                e.mPosition.x = static_cast<float>(value.asInt());
                return value;
            }
            if (member == "y") {
                e.mPosition.y = static_cast<float>(value.asInt());
                return value;
            }
            throw std::runtime_error("Unknown Entity member: " + member);
        }

    private:
        static EntityObject& require(gs::Object& self)
        {
            auto* out = dynamic_cast<EntityObject*>(&self);
            if (!out) {
                throw std::runtime_error("Target is not Entity object");
            }
            return *out;
        }

        const Vec2Type* vec2Type_;
    };

    class ScriptExports
    {
    public:
        ScriptExports()
            : entityType_(vec2Type_) {}

        void Bind(gs::HostRegistry& host)
        {
            host.bind("Vec2", [this](gs::HostContext& ctx, const std::vector<gs::Value>& args) -> gs::Value {
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

            host.bind("Entity", [this](gs::HostContext& ctx, const std::vector<gs::Value>& args) -> gs::Value {
                if (!args.empty()) {
                    throw std::runtime_error("Entity() requires 0 args");
                }
                Entity e;
                e.mName = "Entity";
                e.mHP = 100;
                e.mMP = 50;
                e.mSpeed = 5.0f;
                e.mPosition = Vec2{0.0f, 0.0f};
                return ctx.createObject(std::make_unique<EntityObject>(entityType_, std::move(e)));
            });
        }

    private:
        Vec2Type vec2Type_;
        EntityType entityType_;
    };
}

int main() {
    gs::Runtime runtime;
    MyGame::ScriptExports exports;

    runtime.host().bind("print", [](gs::HostContext&, const std::vector<gs::Value>& args) -> gs::Value {
        if (!args.empty()) {
            std::cout << "[script] " << args[0] << '\n';
        }
        return gs::Value::Int(0);
    });

    exports.Bind(runtime.host());

    const std::string scriptName = "demo.gs";
    const std::vector<std::string> searchPaths = {
        "scripts",
        "../scripts",
        "../../scripts",
        "../../../scripts",
        std::string(GS_PROJECT_ROOT) + "/scripts"
    };

    if (!runtime.loadSourceFile(scriptName, searchPaths)) {
        std::cerr << "Load source failed: " << scriptName << '\n';
        return 1;
    }

    const auto result = runtime.call("main");
    std::cout << "main() -> " << result << '\n';

    auto coroutine = runtime.startCoroutine("tick");
    while (runtime.resumeCoroutine(coroutine, 32) != gs::RunState::Completed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::cout << "tick() -> " << coroutine.returnValue << '\n';

    runtime.saveBytecode("scripts/demo.gsbc");

    std::cout << "Try editing scripts/demo.gs and press Enter to hot reload..." << '\n';
    std::cin.get();
    if (runtime.loadSourceFile(scriptName, searchPaths)) {
        std::cout << "Hot reload success, new main() -> " << runtime.call("main") << '\n';
    } else {
        std::cout << "Hot reload failed" << '\n';
    }

    return 0;
}
