#include "gs/os_module.hpp"
#include "gs/type_system/file_type.hpp"
#include "gs/type_system/path_type.hpp"
#include "gs/type_system/module_type.hpp"
#include "gs/type_system/list_type.hpp"
#include "gs/bytecode.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cstdlib>

namespace gs {

namespace {
    // Global type instances for the module
    std::unique_ptr<FileType> g_fileType;
    std::unique_ptr<PathType> g_pathType;

    // Helper function to convert string/PathObject to filesystem path
    std::string getPathString(HostContext& ctx, const Value& pathArg) {
        if (pathArg.isRef()) {
            Object& obj = ctx.getObject(pathArg);
            if (auto* pathObj = dynamic_cast<PathObject*>(&obj)) {
                return pathObj->path();
            }
        }
        return ctx.__str__(pathArg);
    }

    // Helper function to get FileObject::Mode from string
    FileObject::Mode getModeFromString(const std::string& modeStr) {
        if (modeStr == "r" || modeStr == "read") return FileObject::Mode::Read;
        if (modeStr == "w" || modeStr == "write") return FileObject::Mode::Write;
        if (modeStr == "a" || modeStr == "append") return FileObject::Mode::Append;
        if (modeStr == "rw" || modeStr == "readwrite") return FileObject::Mode::ReadWrite;
        throw std::runtime_error("Invalid file mode: " + modeStr);
    }
}

void registerOsModule(HostRegistry& registry) {
    // Initialize global types
    g_fileType = std::make_unique<FileType>();
    g_pathType = std::make_unique<PathType>();

    // Define the os module
    registry.defineModule("os");

    // === FILE OPERATIONS ===
    
    // open(path, mode="r") -> File
    registry.bindModuleFunction("os", "open", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("open() requires at least 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        std::string mode = args.size() > 1 ? ctx.__str__(args[1]) : "r";
        
        auto fileMode = getModeFromString(mode);
        auto fileObj = std::make_unique<FileObject>(*g_fileType, path, fileMode);
        return ctx.createObject(std::move(fileObj));
    });

    // read(path) -> str
    registry.bindModuleFunction("os", "read", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("read() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + path);
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        return ctx.createString(content);
    });

    // write(path, content) -> int
    registry.bindModuleFunction("os", "write", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) {
            throw std::runtime_error("write() requires 2 arguments");
        }
        
        std::string path = getPathString(ctx, args[0]);
        std::string content = ctx.__str__(args[1]);
        
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for writing: " + path);
        }
        
        file.write(content.c_str(), content.size());
        return Value::Int(static_cast<std::int64_t>(content.size()));
    });

    // append(path, content) -> int
    registry.bindModuleFunction("os", "append", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) {
            throw std::runtime_error("append() requires 2 arguments");
        }
        
        std::string path = getPathString(ctx, args[0]);
        std::string content = ctx.__str__(args[1]);
        
        std::ofstream file(path, std::ios::binary | std::ios::app);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for appending: " + path);
        }
        
        file.write(content.c_str(), content.size());
        return Value::Int(static_cast<std::int64_t>(content.size()));
    });

    // === PATH OPERATIONS ===
    
    // Path(path) -> Path
    registry.bindModuleFunction("os", "Path", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("Path() requires 1 argument");
        }
        
        std::string path = ctx.__str__(args[0]);
        auto pathObj = std::make_unique<PathObject>(*g_pathType, path);
        return ctx.createObject(std::move(pathObj));
    });

    // join(path1, path2, ...) -> str
    registry.bindModuleFunction("os", "join", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("join() requires at least 1 argument");
        }
        
        std::filesystem::path result = getPathString(ctx, args[0]);
        for (std::size_t i = 1; i < args.size(); ++i) {
            result /= getPathString(ctx, args[i]);
        }
        
        return ctx.createString(result.string());
    });

    // abspath(path) -> str
    registry.bindModuleFunction("os", "abspath", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("abspath() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        try {
            auto absolute = std::filesystem::absolute(path);
            return ctx.createString(absolute.string());
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to get absolute path: " + std::string(e.what()));
        }
    });

    // normalize(path) -> str
    registry.bindModuleFunction("os", "normalize", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("normalize() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        auto normalized = std::filesystem::path(path).lexically_normal();
        return ctx.createString(normalized.string());
    });

    // dirname(path) -> str
    registry.bindModuleFunction("os", "dirname", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("dirname() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        auto parent = std::filesystem::path(path).parent_path();
        return ctx.createString(parent.string());
    });

    // basename(path) -> str
    registry.bindModuleFunction("os", "basename", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("basename() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        auto filename = std::filesystem::path(path).filename();
        return ctx.createString(filename.string());
    });

    // extension(path) -> str
    registry.bindModuleFunction("os", "extension", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("extension() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        auto ext = std::filesystem::path(path).extension();
        return ctx.createString(ext.string());
    });

    // === FILE SYSTEM OPERATIONS ===
    
    // exists(path) -> bool
    registry.bindModuleFunction("os", "exists", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("exists() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        return Value::Int(std::filesystem::exists(path) ? 1 : 0);
    });

    // isFile(path) -> bool
    registry.bindModuleFunction("os", "isFile", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("isFile() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        return Value::Int(std::filesystem::is_regular_file(path) ? 1 : 0);
    });

    // isDirectory(path) -> bool
    registry.bindModuleFunction("os", "isDirectory", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("isDirectory() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        return Value::Int(std::filesystem::is_directory(path) ? 1 : 0);
    });

    // fileSize(path) -> int
    registry.bindModuleFunction("os", "fileSize", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("fileSize() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        try {
            auto size = std::filesystem::file_size(path);
            return Value::Int(static_cast<std::int64_t>(size));
        } catch (const std::exception&) {
            return Value::Int(-1);
        }
    });

    // listdir(path) -> list[str]
    registry.bindModuleFunction("os", "listdir", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("listdir() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        std::vector<Value> entries;
        
        try {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                entries.push_back(ctx.createString(entry.path().filename().string()));
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to list directory: " + std::string(e.what()));
        }
        
        // For now, create a simple representation - we need proper list support
        // TODO: Create proper ListObject when available
        if (entries.empty()) {
            return Value::Nil();
        }
        return entries[0]; // Temporary - return first entry
    });

    // remove(path) -> void
    registry.bindModuleFunction("os", "remove", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("remove() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        try {
            if (std::filesystem::is_directory(path)) {
                std::filesystem::remove_all(path);
            } else {
                std::filesystem::remove(path);
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to remove: " + std::string(e.what()));
        }
        
        return Value::Nil();
    });

    // rename(oldPath, newPath) -> void
    registry.bindModuleFunction("os", "rename", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) {
            throw std::runtime_error("rename() requires 2 arguments");
        }
        
        std::string oldPath = getPathString(ctx, args[0]);
        std::string newPath = getPathString(ctx, args[1]);
        
        try {
            std::filesystem::rename(oldPath, newPath);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to rename: " + std::string(e.what()));
        }
        
        return Value::Nil();
    });

    // mkdir(path) -> void
    registry.bindModuleFunction("os", "mkdir", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("mkdir() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        try {
            std::filesystem::create_directories(path);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to create directory: " + std::string(e.what()));
        }
        
        return Value::Nil();
    });

    // getcwd() -> str
    registry.bindModuleFunction("os", "getcwd", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        try {
            auto current = std::filesystem::current_path();
            return ctx.createString(current.string());
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to get current working directory: " + std::string(e.what()));
        }
    });

    // chdir(path) -> void
    registry.bindModuleFunction("os", "chdir", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("chdir() requires 1 argument");
        }
        
        std::string path = getPathString(ctx, args[0]);
        try {
            std::filesystem::current_path(path);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to change directory: " + std::string(e.what()));
        }
        
        return Value::Nil();
    });

    // === CONSTANTS ===
    
    // Add some useful constants
    registry.bindModuleFunction("os", "sep", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        return ctx.createString(std::string(1, std::filesystem::path::preferred_separator));
    });

    // File types for type checking
    registry.bindModuleFunction("os", "FileType", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        // This should return the FileType object for type checking
        // For now, return a string representation
        return ctx.createString("FileType");
    });

    registry.bindModuleFunction("os", "PathType", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        // This should return the PathType object for type checking  
        // For now, return a string representation
        return ctx.createString("PathType");
    });
}

} // namespace gs