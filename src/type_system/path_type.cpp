#include "gs/type_system/path_type.hpp"
#include "gs/type_system/native_function_type.hpp"
#include "gs/type_system/list_type.hpp"
#include "gs/bytecode.hpp"
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <algorithm>

namespace gs {

// PathObject implementation
PathObject::PathObject(const Type& typeRef, const std::string& path)
    : type_(&typeRef), path_(std::filesystem::path(path).string()) {
}

const Type& PathObject::getType() const {
    return *type_;
}

std::string PathObject::extension() const {
    return std::filesystem::path(path_).extension().string();
}

std::string PathObject::filename() const {
    return std::filesystem::path(path_).filename().string();
}

std::string PathObject::stem() const {
    return std::filesystem::path(path_).stem().string();
}

std::string PathObject::parent() const {
    return std::filesystem::path(path_).parent_path().string();
}

bool PathObject::isAbsolute() const {
    return std::filesystem::path(path_).is_absolute();
}

bool PathObject::isRelative() const {
    return std::filesystem::path(path_).is_relative();
}

PathObject PathObject::join(const std::string& other) const {
    auto joined = std::filesystem::path(path_) / other;
    return PathObject(*type_, joined.string());
}

PathObject PathObject::resolve() const {
    try {
        auto absolute = std::filesystem::absolute(path_);
        return PathObject(*type_, absolute.string());
    } catch (const std::exception&) {
        return *this;
    }
}

std::string PathObject::normalize() const {
    try {
        auto normalized = std::filesystem::path(path_).lexically_normal();
        return normalized.string();
    } catch (const std::exception&) {
        return path_;
    }
}

bool PathObject::exists() const {
    return std::filesystem::exists(path_);
}

bool PathObject::isFile() const {
    return std::filesystem::is_regular_file(path_);
}

bool PathObject::isDirectory() const {
    return std::filesystem::is_directory(path_);
}

std::int64_t PathObject::fileSize() const {
    try {
        return static_cast<std::int64_t>(std::filesystem::file_size(path_));
    } catch (const std::exception&) {
        return -1;
    }
}

std::int64_t PathObject::lastModified() const {
    try {
        auto ftime = std::filesystem::last_write_time(path_);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        return std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
    } catch (const std::exception&) {
        return -1;
    }
}

std::vector<PathObject> PathObject::listDir() const {
    std::vector<PathObject> result;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path_)) {
            result.emplace_back(*type_, entry.path().string());
        }
    } catch (const std::exception&) {
        // Return empty vector on error
    }
    return result;
}

// PathType implementation
PathType::PathType() {
    initializeSlots();
}

std::string PathType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto& path = requirePath(self);
    return path.toString();
}

PathObject& PathType::requirePath(Object& self) const {
    auto* path = dynamic_cast<PathObject*>(&self);
    if (!path) {
        throw std::runtime_error("Expected PathObject, got " + std::string(self.getType().name()));
    }
    return *path;
}

void PathType::initializeSlots() {
    // Path methods using the proper registration system
    registerMethodAttribute("extension", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return makeString(requirePath(self).extension());
    });
    
    registerMethodAttribute("filename", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return makeString(requirePath(self).filename());
    });
    
    registerMethodAttribute("stem", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return makeString(requirePath(self).stem());
    });
    
    registerMethodAttribute("normalize", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return makeString(requirePath(self).normalize());
    });
    
    registerMethodAttribute("exists", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return Value::Int(requirePath(self).exists() ? 1 : 0);
    });
    
    registerMethodAttribute("isFile", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return Value::Int(requirePath(self).isFile() ? 1 : 0);
    });
    
    registerMethodAttribute("isDirectory", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return Value::Int(requirePath(self).isDirectory() ? 1 : 0);
    });
    
    registerMethodAttribute("fileSize", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return Value::Int(requirePath(self).fileSize());
    });
    
    registerMethodAttribute("lastModified", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return Value::Int(requirePath(self).lastModified());
    });
}

} // namespace gs