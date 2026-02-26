#pragma once

#include "gs/type_system/type_base.hpp"
#include <string>
#include <vector>

namespace gs {

// Path object representing a filesystem path
class PathObject : public Object {
public:
    PathObject(const Type& typeRef, const std::string& path);
    ~PathObject() override = default;
    
    const Type& getType() const override;
    
    // Path operations
    std::string toString() const { return path_; }
    std::string extension() const;
    std::string filename() const;
    std::string stem() const; // filename without extension
    std::string parent() const;
    bool isAbsolute() const;
    bool isRelative() const;
    PathObject join(const std::string& other) const;
    PathObject resolve() const; // resolve to absolute path
    std::string normalize() const; // normalize path separators
    
    // Filesystem operations
    bool exists() const;
    bool isFile() const;
    bool isDirectory() const;
    std::int64_t fileSize() const;
    std::int64_t lastModified() const;
    std::vector<PathObject> listDir() const;
    
    const std::string& path() const { return path_; }
    
private:
    const Type* type_;
    std::string path_;
};

// Path type
class PathType : public Type {
public:
    PathType();
    
    const char* name() const override { return "path"; }
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
    
    // Type-specific methods
    PathObject& requirePath(Object& self) const;
    
private:
    void initializeSlots();
};

} // namespace gs