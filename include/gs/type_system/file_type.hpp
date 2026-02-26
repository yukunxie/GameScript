#pragma once

#include "gs/type_system/type_base.hpp"
#include <string>
#include <fstream>
#include <memory>

namespace gs {

// File object representing an open file handle
class FileObject : public Object {
public:
    enum class Mode {
        Read,
        Write,
        Append,
        ReadWrite
    };

    FileObject(const Type& typeRef, const std::string& path, Mode mode);
    ~FileObject() override;
    
    const Type& getType() const override;
    
    // File operations
    bool isOpen() const;
    void close();
    std::string read(std::size_t count = SIZE_MAX);
    std::string readLine();
    std::int64_t write(const std::string& data);
    void flush();
    std::int64_t seek(std::int64_t offset, int whence = 0); // 0=SEEK_SET, 1=SEEK_CUR, 2=SEEK_END
    std::int64_t tell();
    std::int64_t size();
    
    const std::string& path() const { return path_; }
    Mode mode() const { return mode_; }
    
private:
    const Type* type_;
    std::string path_;
    Mode mode_;
    std::unique_ptr<std::fstream> stream_;
};

// File type
class FileType : public Type {
public:
    FileType();
    
    const char* name() const override { return "file"; }
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
    
    // Type-specific methods
    FileObject& requireFile(Object& self) const;
    
private:
    void initializeSlots();
};

} // namespace gs