#include "gs/type_system/exception_type.hpp"

#include <sstream>
#include <stdexcept>

namespace gs {

const std::string_view kExceptionTypeName = "Exception";
const std::string_view kDivideByZeroExceptionTypeName = "DivideByZeroException";
const std::string_view kFileNotFoundExceptionTypeName = "FileNotFoundException";
const std::string_view kFileReadExceptionTypeName = "FileReadException";
const std::string_view kPropertyNotFoundExceptionTypeName = "PropertyNotFoundException";
const std::string_view kListIndexOutOfRangeExceptionTypeName = "ListIndexOutOfRangeException";
const std::string_view kDictKeyNotFoundExceptionTypeName = "DictKeyNotFoundException";
const std::string_view kUndefinedVariableExceptionTypeName = "UndefinedVariableException";

ExceptionObject::ExceptionObject(const Type& typeRef, std::string exceptionName, std::string message)
    : type_(&typeRef),
      exceptionName_(std::move(exceptionName)),
      message_(std::move(message)) {}

const Type& ExceptionObject::getType() const {
    return *type_;
}

const std::string& ExceptionObject::exceptionName() const {
    return exceptionName_;
}

const std::string& ExceptionObject::message() const {
    return message_;
}

void ExceptionObject::setExceptionName(std::string exceptionName) {
    exceptionName_ = std::move(exceptionName);
}

void ExceptionObject::setMessage(std::string message) {
    message_ = std::move(message);
}

const std::vector<ExceptionStackFrame>& ExceptionObject::stackTrace() const {
    return stackTrace_;
}

bool ExceptionObject::hasStackTrace() const {
    return !stackTrace_.empty();
}

void ExceptionObject::setStackTrace(std::vector<ExceptionStackFrame> stackTrace) {
    stackTrace_ = std::move(stackTrace);
}

std::string ExceptionObject::formatStackTrace() const {
    std::ostringstream out;
    for (std::size_t i = 0; i < stackTrace_.size(); ++i) {
        const auto& frame = stackTrace_[i];
        if (i > 0) {
            out << "\n";
        }
        out << '[' << i << "] " << frame.functionName
            << " (line " << frame.line
            << ", column " << frame.column
            << ", ip=" << frame.ip << ')';
    }
    return out.str();
}

ExceptionType::ExceptionType() {
    registerMethodAttribute("what", 0, [this](Object& self,
                                                const std::vector<Value>& args,
                                                const StringFactory& makeString,
                                                const ValueStrInvoker& valueStr) {
        (void)args;
        (void)valueStr;
        auto& exception = requireException(self);
        return makeString(exception.message());
    });

    registerMethodAttribute("throw_function", 0, [this](Object& self,
                                                         const std::vector<Value>& args,
                                                         const StringFactory& makeString,
                                                         const ValueStrInvoker& valueStr) {
        (void)args;
        (void)valueStr;
        auto& exception = requireException(self);
        if (!exception.hasStackTrace()) {
            return makeString("");
        }
        return makeString(exception.stackTrace().front().functionName);
    });

    registerMethodAttribute("throw_line", 0, [this](Object& self,
                                                     const std::vector<Value>& args,
                                                     const StringFactory& makeString,
                                                     const ValueStrInvoker& valueStr) {
        (void)args;
        (void)makeString;
        (void)valueStr;
        auto& exception = requireException(self);
        if (!exception.hasStackTrace()) {
            return Value::Int(0);
        }
        return Value::Int(static_cast<std::int64_t>(exception.stackTrace().front().line));
    });

    registerMethodAttribute("throw_column", 0, [this](Object& self,
                                                       const std::vector<Value>& args,
                                                       const StringFactory& makeString,
                                                       const ValueStrInvoker& valueStr) {
        (void)args;
        (void)makeString;
        (void)valueStr;
        auto& exception = requireException(self);
        if (!exception.hasStackTrace()) {
            return Value::Int(0);
        }
        return Value::Int(static_cast<std::int64_t>(exception.stackTrace().front().column));
    });

    registerMethodAttribute("throw_stack", 0, [this](Object& self,
                                                      const std::vector<Value>& args,
                                                      const StringFactory& makeString,
                                                      const ValueStrInvoker& valueStr) {
        (void)args;
        (void)valueStr;
        auto& exception = requireException(self);
        return makeString(exception.formatStackTrace());
    });
}

const char* ExceptionType::name() const {
    return "Exception";
}

std::string ExceptionType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    (void)valueStr;
    auto& exception = requireException(self);
    if (exception.message().empty()) {
        return exception.exceptionName();
    }
    return exception.exceptionName() + ": " + exception.message();
}

ExceptionObject& ExceptionType::requireException(Object& self) const {
    auto* exception = dynamic_cast<ExceptionObject*>(&self);
    if (!exception) {
        throw std::runtime_error("ExceptionType called with non-exception object");
    }
    return *exception;
}

DivideByZeroExceptionObject::DivideByZeroExceptionObject(const Type& typeRef, std::string message)
    : ExceptionObject(typeRef, std::string(kDivideByZeroExceptionTypeName), std::move(message)) {}

FileNotFoundExceptionObject::FileNotFoundExceptionObject(const Type& typeRef, std::string message)
    : ExceptionObject(typeRef, std::string(kFileNotFoundExceptionTypeName), std::move(message)) {}

FileReadExceptionObject::FileReadExceptionObject(const Type& typeRef, std::string message)
    : ExceptionObject(typeRef, std::string(kFileReadExceptionTypeName), std::move(message)) {}

PropertyNotFoundExceptionObject::PropertyNotFoundExceptionObject(const Type& typeRef, std::string message)
    : ExceptionObject(typeRef, std::string(kPropertyNotFoundExceptionTypeName), std::move(message)) {}

ListIndexOutOfRangeExceptionObject::ListIndexOutOfRangeExceptionObject(const Type& typeRef, std::string message)
    : ExceptionObject(typeRef, std::string(kListIndexOutOfRangeExceptionTypeName), std::move(message)) {}

DictKeyNotFoundExceptionObject::DictKeyNotFoundExceptionObject(const Type& typeRef, std::string message)
    : ExceptionObject(typeRef, std::string(kDictKeyNotFoundExceptionTypeName), std::move(message)) {}

UndefinedVariableExceptionObject::UndefinedVariableExceptionObject(const Type& typeRef, std::string message)
    : ExceptionObject(typeRef, std::string(kUndefinedVariableExceptionTypeName), std::move(message)) {}

const char* DivideByZeroExceptionType::name() const {
    return "DivideByZeroException";
}

const char* FileNotFoundExceptionType::name() const {
    return "FileNotFoundException";
}

const char* FileReadExceptionType::name() const {
    return "FileReadException";
}

const char* PropertyNotFoundExceptionType::name() const {
    return "PropertyNotFoundException";
}

const char* ListIndexOutOfRangeExceptionType::name() const {
    return "ListIndexOutOfRangeException";
}

const char* DictKeyNotFoundExceptionType::name() const {
    return "DictKeyNotFoundException";
}

const char* UndefinedVariableExceptionType::name() const {
    return "UndefinedVariableException";
}

bool isNativeExceptionTypeName(const std::string& nativeTypeName) {
    return nativeTypeName == kExceptionTypeName ||
           nativeTypeName == kDivideByZeroExceptionTypeName ||
           nativeTypeName == kFileNotFoundExceptionTypeName ||
           nativeTypeName == kFileReadExceptionTypeName ||
           nativeTypeName == kPropertyNotFoundExceptionTypeName ||
           nativeTypeName == kListIndexOutOfRangeExceptionTypeName ||
           nativeTypeName == kDictKeyNotFoundExceptionTypeName ||
           nativeTypeName == kUndefinedVariableExceptionTypeName;
}

std::string classifyRuntimeExceptionTypeName(const std::string& message) {
    if (message == "Division by zero" || message == "Modulo by zero") {
        return std::string(kDivideByZeroExceptionTypeName);
    }
    if (message.starts_with("Failed to open file:")) {
        return std::string(kFileNotFoundExceptionTypeName);
    }
    if (message.starts_with("File is not open") || message.starts_with("Failed to read file:")) {
        return std::string(kFileReadExceptionTypeName);
    }
    if (message.starts_with("Unknown class attribute:") ||
        message.starts_with("Unknown member") ||
        message.starts_with("Unknown or read-only")) {
        return std::string(kPropertyNotFoundExceptionTypeName);
    }
    if (message.find("List") != std::string::npos && message.find("out of range") != std::string::npos) {
        return std::string(kListIndexOutOfRangeExceptionTypeName);
    }
    if (message.find("Dict") != std::string::npos && message.find("key") != std::string::npos &&
        message.find("not found") != std::string::npos) {
        return std::string(kDictKeyNotFoundExceptionTypeName);
    }
    if (message.starts_with("Undefined symbol:")) {
        return std::string(kUndefinedVariableExceptionTypeName);
    }
    return std::string(kExceptionTypeName);
}

const Type& resolveNativeExceptionType(std::string_view exceptionName) {
    static ExceptionType baseType;
    static DivideByZeroExceptionType divideByZeroType;
    static FileNotFoundExceptionType fileNotFoundType;
    static FileReadExceptionType fileReadType;
    static PropertyNotFoundExceptionType propertyNotFoundType;
    static ListIndexOutOfRangeExceptionType listIndexOutOfRangeType;
    static DictKeyNotFoundExceptionType dictKeyNotFoundType;
    static UndefinedVariableExceptionType undefinedVariableType;

    if (exceptionName == kDivideByZeroExceptionTypeName) {
        return divideByZeroType;
    }
    if (exceptionName == kFileNotFoundExceptionTypeName) {
        return fileNotFoundType;
    }
    if (exceptionName == kFileReadExceptionTypeName) {
        return fileReadType;
    }
    if (exceptionName == kPropertyNotFoundExceptionTypeName) {
        return propertyNotFoundType;
    }
    if (exceptionName == kListIndexOutOfRangeExceptionTypeName) {
        return listIndexOutOfRangeType;
    }
    if (exceptionName == kDictKeyNotFoundExceptionTypeName) {
        return dictKeyNotFoundType;
    }
    if (exceptionName == kUndefinedVariableExceptionTypeName) {
        return undefinedVariableType;
    }
    return baseType;
}

std::unique_ptr<ExceptionObject> makeNativeExceptionObject(std::string_view exceptionName,
                                                           const std::string& message) {
    const Type& exceptionType = resolveNativeExceptionType(exceptionName);
    if (exceptionName == kDivideByZeroExceptionTypeName) {
        return std::make_unique<DivideByZeroExceptionObject>(exceptionType, message);
    }
    if (exceptionName == kFileNotFoundExceptionTypeName) {
        return std::make_unique<FileNotFoundExceptionObject>(exceptionType, message);
    }
    if (exceptionName == kFileReadExceptionTypeName) {
        return std::make_unique<FileReadExceptionObject>(exceptionType, message);
    }
    if (exceptionName == kPropertyNotFoundExceptionTypeName) {
        return std::make_unique<PropertyNotFoundExceptionObject>(exceptionType, message);
    }
    if (exceptionName == kListIndexOutOfRangeExceptionTypeName) {
        return std::make_unique<ListIndexOutOfRangeExceptionObject>(exceptionType, message);
    }
    if (exceptionName == kDictKeyNotFoundExceptionTypeName) {
        return std::make_unique<DictKeyNotFoundExceptionObject>(exceptionType, message);
    }
    if (exceptionName == kUndefinedVariableExceptionTypeName) {
        return std::make_unique<UndefinedVariableExceptionObject>(exceptionType, message);
    }
    return std::make_unique<ExceptionObject>(exceptionType, std::string(exceptionName), message);
}

} // namespace gs
