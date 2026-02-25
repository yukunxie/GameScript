#include "gs/runtime.hpp"
#include "gs/bound_class_type.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

// ============================================================================
// TypeConverter for custom types must be defined before use
// ============================================================================

namespace gs::detail {
    // Forward declare Vec2
    struct Vec2;
}

namespace MyGame
{
    // ========================================================================
    // C++ Game Types (Plain C++ structs/classes)
    // ========================================================================
    
    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;

        Vec2() = default;
        Vec2(float x, float y) : x(x), y(y) {}

        // Methods that will be bound to script
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

    class Entity
    {
    public:
        std::string mName;
        std::uint32_t mHP = 100;
        std::uint32_t mMP = 50;
        float mSpeed = 5.0f;
        Vec2 mPosition;

        Entity() = default;

        void GotoPoint(const Vec2& p) {
            mPosition = p;
            // TODO: actual movement logic
        }

        int GetDataSize() const {
            std::vector<int> data;
            // TODO: fill data
            return static_cast<int>(data.size());
        }

        // Accessors for binding
        Vec2 getPosition() const { return mPosition; }
        void setPosition(const Vec2& pos) { mPosition = pos; }
    };

    // ========================================================================
    // Script Binding (New V2 Style - Clean & Simple!)
    // ========================================================================

    class ScriptExports
    {
    public:
        void Bind(gs::HostRegistry& host)
        {
            std::cout << "[C++] Starting Bind()..." << std::endl;
            
            // TODO: Integrate with proper type system
            // For now, create placeholder types (must be non-const for attribute registration)
            static gs::BoundClassType vec2Type("Vec2");
            static gs::BoundClassType entityType("Entity");
            
            // Register types with non-const references
            gs::registerNativeType(typeid(Vec2), vec2Type);
            gs::registerNativeType(typeid(Entity), entityType);
            
            std::cout << "[C++] Types registered" << std::endl;
            
            // Create binding context with the new V2 API
            gs::BindingContext bindings(host);
            
            std::cout << "[C++] BindingContext created" << std::endl;

            // ----------------------------------------------------------------
            // Bind Vec2 (5 lines instead of 120+ lines!)
            // ----------------------------------------------------------------
            std::cout << "[C++] Binding Vec2..." << std::endl;
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
            
            std::cout << "[C++] Binding Entity..." << std::endl;
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

            std::cout << "[C++] Binding Distance function..." << std::endl;
            bindings.function("Distance", [](const Vec2& a, const Vec2& b) -> float {
                const float dx = a.x - b.x;
                const float dy = a.y - b.y;
                return std::sqrt(dx * dx + dy * dy);
            });
            
            std::cout << "[C++] Bind() completed" << std::endl;
        }
    };
}

namespace {

void printErrorSourceCaret(const std::string& diagnostic) {
    static const std::regex clangStyleRe(R"(^(.*):(\d+):(\d+):\s*error:.*$)");
    std::smatch match;
    if (!std::regex_match(diagnostic, match, clangStyleRe)) {
        return;
    }

    const std::filesystem::path filePath = match[1].str();
    const std::size_t lineNo = static_cast<std::size_t>(std::stoull(match[2].str()));
    const std::size_t columnNo = static_cast<std::size_t>(std::stoull(match[3].str()));

    std::ifstream input(filePath);
    if (!input) {
        return;
    }

    std::string line;
    for (std::size_t current = 1; current <= lineNo; ++current) {
        if (!std::getline(input, line)) {
            return;
        }
    }

    const std::string linePrefix = std::to_string(lineNo) + " | ";
    std::cerr << linePrefix << line << '\n';
    const std::size_t caretPos = columnNo > 0 ? (columnNo - 1) : 0;
    std::cerr << std::string(linePrefix.size() + caretPos, ' ') << "^" << '\n';
}

} // namespace

int main() {
    std::cout << "[C++] Creating runtime..." << std::endl;
    gs::Runtime runtime;
    std::cout << "[C++] Runtime created" << std::endl;
    
    MyGame::ScriptExports exports;
    std::cout << "[C++] Calling Bind()..." << std::endl;
    exports.Bind(runtime.host());
    std::cout << "[C++] Bind() returned" << std::endl;

    const std::string scriptName = "demo.gs";
    const std::vector<std::string> searchPaths = {
        "scripts",
        "../scripts",
        "../../scripts",
        "../../../scripts",
        std::string(GS_PROJECT_ROOT) + "/scripts"
    };

    std::cout << "[C++] Loading script..." << std::endl;
    
    bool loaded = false;
    try {
        loaded = runtime.loadSourceFile(scriptName, searchPaths);
    } catch (const std::exception& e) {
        std::cerr << "[C++] Exception during loadSourceFile: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[C++] Unknown exception during loadSourceFile" << std::endl;
        return 1;
    }
    
    if (!loaded) {
        std::cerr << "Load source failed: " << scriptName;
        if (!runtime.lastError().empty()) {
            std::cerr << "\n" << runtime.lastError();
            std::cerr << '\n';
            printErrorSourceCaret(runtime.lastError());
        }
        std::cerr << '\n';
        return 1;
    }
    
    std::cout << "[C++] Script loaded successfully!" << std::endl;
    std::cout << "[C++] Calling main()..." << std::endl;
    std::cout.flush();

    const auto result = runtime.call("main");
    std::cout << "main() -> " << result << std::endl;

    // std::cout << "tick() -> " << runtime.call("tick") << '\n';

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
