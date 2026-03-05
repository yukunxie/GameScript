#pragma once

#include "gs/type_system/type_base.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace gs {

struct ExceptionStackFrame {
    std::string functionName;
    std::size_t line{0};
    std::size_t column{0};
    std::size_t ip{0};
};

class ExceptionObject : public Object {
public:
    ExceptionObject(const Type& typeRef, std::string exceptionName, std::string message);

    const Type& getType() const override;

    const std::string& exceptionName() const;
    const std::string& message() const;
    void setExceptionName(std::string exceptionName);
    void setMessage(std::string message);
    const std::vector<ExceptionStackFrame>& stackTrace() const;
    bool hasStackTrace() const;
    void setStackTrace(std::vector<ExceptionStackFrame> stackTrace);
    std::string formatStackTrace() const;

private:
    const Type* type_;
    std::string exceptionName_;
    std::string message_;
    std::vector<ExceptionStackFrame> stackTrace_;
};

class ExceptionType : public Type {
public:
    ExceptionType();

    const char* name() const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;

private:
    ExceptionObject& requireException(Object& self) const;
};

class DivideByZeroExceptionObject final : public ExceptionObject {
public:
    DivideByZeroExceptionObject(const Type& typeRef, std::string message);
};

class FileNotFoundExceptionObject final : public ExceptionObject {
public:
    FileNotFoundExceptionObject(const Type& typeRef, std::string message);
};

class FileReadExceptionObject final : public ExceptionObject {
public:
    FileReadExceptionObject(const Type& typeRef, std::string message);
};

class PropertyNotFoundExceptionObject final : public ExceptionObject {
public:
    PropertyNotFoundExceptionObject(const Type& typeRef, std::string message);
};

class ListIndexOutOfRangeExceptionObject final : public ExceptionObject {
public:
    ListIndexOutOfRangeExceptionObject(const Type& typeRef, std::string message);
};

class DictKeyNotFoundExceptionObject final : public ExceptionObject {
public:
    DictKeyNotFoundExceptionObject(const Type& typeRef, std::string message);
};

class UndefinedVariableExceptionObject final : public ExceptionObject {
public:
    UndefinedVariableExceptionObject(const Type& typeRef, std::string message);
};

class DivideByZeroExceptionType final : public ExceptionType {
public:
    const char* name() const override;
};

class FileNotFoundExceptionType final : public ExceptionType {
public:
    const char* name() const override;
};

class FileReadExceptionType final : public ExceptionType {
public:
    const char* name() const override;
};

class PropertyNotFoundExceptionType final : public ExceptionType {
public:
    const char* name() const override;
};

class ListIndexOutOfRangeExceptionType final : public ExceptionType {
public:
    const char* name() const override;
};

class DictKeyNotFoundExceptionType final : public ExceptionType {
public:
    const char* name() const override;
};

class UndefinedVariableExceptionType final : public ExceptionType {
public:
    const char* name() const override;
};

extern const std::string_view kExceptionTypeName;
extern const std::string_view kDivideByZeroExceptionTypeName;
extern const std::string_view kFileNotFoundExceptionTypeName;
extern const std::string_view kFileReadExceptionTypeName;
extern const std::string_view kPropertyNotFoundExceptionTypeName;
extern const std::string_view kListIndexOutOfRangeExceptionTypeName;
extern const std::string_view kDictKeyNotFoundExceptionTypeName;
extern const std::string_view kUndefinedVariableExceptionTypeName;

bool isNativeExceptionTypeName(const std::string& nativeTypeName);
std::string classifyRuntimeExceptionTypeName(const std::string& message);
const Type& resolveNativeExceptionType(std::string_view exceptionName);
std::unique_ptr<ExceptionObject> makeNativeExceptionObject(std::string_view exceptionName,
                                                           const std::string& message);

} // namespace gs
