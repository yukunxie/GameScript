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
#define GS_EXPORT_METHOD(NAME, ARGC, FN)                                                                  \
    registerMethodAttribute(NAME, ARGC, [this](gs::Object& self, const std::vector<gs::Value>& args) { \
        return FN(self, args);                                                                            \
    })

#define GS_EXPECT_ARGC(TYPE_NAME, METHOD_NAME, ACTUAL, EXPECTED)                                           \
    do {                                                                                                     \
        if ((ACTUAL) != (EXPECTED)) {                                                                        \
            throw std::runtime_error(std::string(TYPE_NAME) + "." + METHOD_NAME +                          \
                                     "() argument count mismatch, expected " + std::to_string(EXPECTED));  \
        }                                                                                                    \
    } while (0)

#define GS_EXPORT_NUM_MEMBER(NAME, TARGET_EXPR, CASTTYPE)                                                    \
    registerMemberAttribute(                                                                                  \
        NAME,                                                                                                 \
        [this](gs::Object& self) {                                                                            \
            return gs::Value::Int(static_cast<std::int64_t>(TARGET_EXPR));                                   \
        },                                                                                                    \
        [this](gs::Object& self, const gs::Value& value) {                                                   \
            TARGET_EXPR = static_cast<CASTTYPE>(value.asInt());                                               \
            return value;                                                                                     \
        })

#define GS_EXPECT_REF(TYPE_NAME, MEMBER_NAME, VALUE_EXPR)                                                  \
    do {                                                                                                    \
        if (!(VALUE_EXPR).isRef()) {                                                                       \
            throw std::runtime_error(std::string(TYPE_NAME) + "." + MEMBER_NAME +                        \
                                     " requires object reference");                                        \
        }                                                                                                   \
    } while (0)

#define GS_EXPORT_OBJECT_MEMBER(NAME, GET_EXPR, SET_EXPR)                                                   \
    registerMemberAttribute(                                                                                 \
        NAME,                                                                                                \
        [this](gs::Object& self) {                                                                           \
            return (GET_EXPR);                                                                               \
        },                                                                                                   \
        [this](gs::Object& self, const gs::Value& value) {                                                  \
            SET_EXPR;                                                                                        \
            return value;                                                                                    \
        })

    struct Vec2
    {
        Vec2() = default;
        Vec2(float x, float y)
            : x(x)
            , y(y)
        {
        }
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
        EntityObject(const gs::Type& typeRef, Entity value, gs::Value positionRef)
            : type_(&typeRef), value_(std::move(value)), positionRef_(positionRef) {}

        const gs::Type& getType() const override { return *type_; }
        Entity& value() { return value_; }
        const Entity& value() const { return value_; }
        gs::Value positionRef() const { return positionRef_; }
        void setPositionRef(gs::Value ref) { positionRef_ = ref; }

    private:
        const gs::Type* type_;
        Entity value_;
        gs::Value positionRef_{gs::Value::Nil()};
    };

    class Vec2Type : public gs::Type
    {
    public:
        Vec2Type()
        {
            GS_EXPORT_METHOD("length", 0, methodLength);
            GS_EXPORT_NUM_MEMBER("x", require(self).value().x, float);
            GS_EXPORT_NUM_MEMBER("y", require(self).value().y, float);
        }

        const char* name() const override { return "Vec2"; }
        std::string __str__(gs::Object& self, const gs::Type::ValueStrInvoker& valueStr) const override
        {
            (void)valueStr;
            auto& vecObj = require(self);
            const auto& vec = vecObj.value();
            return "Vec2(" + std::to_string(vec.x) + ", " + std::to_string(vec.y) + ")";
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

        gs::Value methodLength(gs::Object& self, const std::vector<gs::Value>& args) const
        {
            GS_EXPECT_ARGC("Vec2", "length", args.size(), 0);
            const auto& vec = require(self).value();
            const auto len = std::sqrt(static_cast<double>(vec.x * vec.x + vec.y * vec.y));
            return gs::Value::Int(static_cast<std::int64_t>(len));
        }

    };

    class EntityType : public gs::Type
    {
    public:
        explicit EntityType(const Vec2Type& vec2Type)
            : vec2Type_(&vec2Type) {}

        EntityType()
            : vec2Type_(nullptr)
        {
            registerExports();
        }

        explicit EntityType(const Vec2Type& vec2Type, bool)
            : vec2Type_(&vec2Type)
        {
            registerExports();
        }

        const char* name() const override { return "Entity"; }

    private:
        void registerExports()
        {
            GS_EXPORT_METHOD("GotoPoint", 1, methodGotoPoint);
            GS_EXPORT_METHOD("GetData", 0, methodGetData);

            GS_EXPORT_NUM_MEMBER("HP", require(self).value().mHP, std::uint32_t);
            GS_EXPORT_NUM_MEMBER("MP", require(self).value().mMP, std::uint32_t);
            GS_EXPORT_NUM_MEMBER("Speed", require(self).value().mSpeed, float);
            GS_EXPORT_OBJECT_MEMBER("Position",
                                    require(self).positionRef(),
                                    GS_EXPECT_REF("Entity", "Position", value);
                                    require(self).setPositionRef(value));
        }

        static EntityObject& require(gs::Object& self)
        {
            auto* out = dynamic_cast<EntityObject*>(&self);
            if (!out) {
                throw std::runtime_error("Target is not Entity object");
            }
            return *out;
        }

        gs::Value methodGotoPoint(gs::Object& self, const std::vector<gs::Value>& args) const
        {
            GS_EXPECT_ARGC("Entity", "GotoPoint", args.size(), 1);
            if (!args[0].isRef()) {
                throw std::runtime_error("Entity.GotoPoint requires Vec2 object argument");
            }

            auto& entityObj = require(self);
            entityObj.setPositionRef(args[0]);
            entityObj.value().GotoPoint(entityObj.value().mPosition);
            return gs::Value::Int(0);
        }

        gs::Value methodGetData(gs::Object& self, const std::vector<gs::Value>& args) const
        {
            GS_EXPECT_ARGC("Entity", "GetData", args.size(), 0);
            auto& entity = require(self).value();
            return gs::Value::Int(static_cast<std::int64_t>(entity.GetData().size()));
        }

        const Vec2Type* vec2Type_;
    };

    class ScriptExports
    {
    public:
        ScriptExports()
            : entityType_(vec2Type_, true) {}

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
                const auto positionRef = ctx.createObject(std::make_unique<Vec2Object>(vec2Type_, e.mPosition));
                return ctx.createObject(std::make_unique<EntityObject>(entityType_, std::move(e), positionRef));
            });
        }

    private:
        Vec2Type vec2Type_;
        EntityType entityType_;
    };

#undef GS_EXPECT_ARGC
#undef GS_EXPORT_OBJECT_MEMBER
#undef GS_EXPECT_REF
#undef GS_EXPORT_NUM_MEMBER
#undef GS_EXPORT_METHOD
}

int main() {
    gs::Runtime runtime;
    MyGame::ScriptExports exports;

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

    /*std::cout << "Try editing scripts/demo.gs and press Enter to hot reload..." << '\n';
    std::cin.get();
    if (runtime.loadSourceFile(scriptName, searchPaths)) {
        std::cout << "Hot reload success, new main() -> " << runtime.call("main") << '\n';
    } else {
        std::cout << "Hot reload failed" << '\n';
    }*/

    return 0;
}
