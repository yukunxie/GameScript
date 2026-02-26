#include "gs/type_system/file_type.hpp"
#include "gs/type_system/native_function_type.hpp"
#include "gs/bytecode.hpp"
#include <iostream>
#include <stdexcept>
#include <filesystem>

namespace gs {

// FileObject implementation
FileObject::FileObject(const Type& typeRef, const std::string& path, Mode mode)
    : type_(&typeRef), path_(path), mode_(mode) {
    
    std::ios::openmode openMode = std::ios::binary;
    switch (mode) {
        case Mode::Read:
            openMode |= std::ios::in;
            break;
        case Mode::Write:
            openMode |= std::ios::out | std::ios::trunc;
            break;
        case Mode::Append:
            openMode |= std::ios::out | std::ios::app;
            break;
        case Mode::ReadWrite:
            openMode |= std::ios::in | std::ios::out;
            break;
    }
    
    stream_ = std::make_unique<std::fstream>(path, openMode);
    if (!stream_->is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
}

FileObject::~FileObject() {
    if (stream_ && stream_->is_open()) {
        stream_->close();
    }
}

const Type& FileObject::getType() const {
    return *type_;
}

bool FileObject::isOpen() const {
    return stream_ && stream_->is_open();
}

void FileObject::close() {
    if (stream_ && stream_->is_open()) {
        stream_->close();
    }
}

std::string FileObject::read(std::size_t count) {
    if (!isOpen()) {
        throw std::runtime_error("File is not open");
    }
    
    if (count == SIZE_MAX) {
        // Read entire file
        stream_->seekg(0, std::ios::end);
        std::size_t size = stream_->tellg();
        stream_->seekg(0, std::ios::beg);
        
        std::string content(size, '\0');
        stream_->read(&content[0], size);
        return content;
    } else {
        std::string content(count, '\0');
        stream_->read(&content[0], count);
        content.resize(stream_->gcount());
        return content;
    }
}

std::string FileObject::readLine() {
    if (!isOpen()) {
        throw std::runtime_error("File is not open");
    }
    
    std::string line;
    std::getline(*stream_, line);
    return line;
}

std::int64_t FileObject::write(const std::string& data) {
    if (!isOpen()) {
        throw std::runtime_error("File is not open");
    }
    
    stream_->write(data.c_str(), data.size());
    return data.size();
}

void FileObject::flush() {
    if (isOpen()) {
        stream_->flush();
    }
}

std::int64_t FileObject::seek(std::int64_t offset, int whence) {
    if (!isOpen()) {
        throw std::runtime_error("File is not open");
    }
    
    std::ios::seekdir dir;
    switch (whence) {
        case 0: dir = std::ios::beg; break;
        case 1: dir = std::ios::cur; break;
        case 2: dir = std::ios::end; break;
        default: throw std::runtime_error("Invalid seek whence value");
    }
    
    stream_->seekg(offset, dir);
    stream_->seekp(offset, dir);
    return stream_->tellg();
}

std::int64_t FileObject::tell() {
    if (!isOpen()) {
        throw std::runtime_error("File is not open");
    }
    
    return stream_->tellg();
}

std::int64_t FileObject::size() {
    if (!isOpen()) {
        throw std::runtime_error("File is not open");
    }
    
    auto current = stream_->tellg();
    stream_->seekg(0, std::ios::end);
    auto size = stream_->tellg();
    stream_->seekg(current);
    return size;
}

// FileType implementation
FileType::FileType() {
    initializeSlots();
}

std::string FileType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto& file = requireFile(self);
    return "<file '" + file.path() + "'>";
}

FileObject& FileType::requireFile(Object& self) const {
    auto* file = dynamic_cast<FileObject*>(&self);
    if (!file) {
        throw std::runtime_error("Expected FileObject, got " + std::string(self.getType().name()));
    }
    return *file;
}

void FileType::initializeSlots() {
    // File methods using the proper registration system
    registerMethodAttribute("close", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        requireFile(self).close();
        return Value::Nil();
    });
    
    registerMethodAttribute("read", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        std::size_t count = SIZE_MAX;
        if (!args.empty()) {
            count = static_cast<std::size_t>(args[0].asInt());
        }
        return makeString(requireFile(self).read(count));
    });
    
    registerMethodAttribute("readLine", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return makeString(requireFile(self).readLine());
    });
    
    registerMethodAttribute("write", 1, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        if (args.empty()) {
            throw std::runtime_error("write() requires at least 1 argument");
        }
        std::string data = valueStr(args[0]);
        return Value::Int(requireFile(self).write(data));
    });
    
    registerMethodAttribute("flush", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        requireFile(self).flush();
        return Value::Nil();
    });
    
    registerMethodAttribute("seek", 1, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        if (args.empty()) {
            throw std::runtime_error("seek() requires at least 1 argument");
        }
        std::int64_t offset = args[0].asInt();
        int whence = args.size() > 1 ? static_cast<int>(args[1].asInt()) : 0;
        return Value::Int(requireFile(self).seek(offset, whence));
    });
    
    registerMethodAttribute("tell", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return Value::Int(requireFile(self).tell());
    });
    
    registerMethodAttribute("size", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return Value::Int(requireFile(self).size());
    });
    
    registerMethodAttribute("isOpen", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return Value::Int(requireFile(self).isOpen() ? 1 : 0);
    });
    
    registerMemberAttribute("path", 
        [this](Object& self) -> Value { 
            return Value::Nil(); // Will need proper string creation in context
        },
        [this](Object& self, const Value& value) -> Value {
            throw std::runtime_error("path is read-only");
        });
}

} // namespace gs