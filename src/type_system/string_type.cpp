#include "gs/type_system/string_type.hpp"
#include "gs/type_system/list_type.hpp"
#include "gs/bytecode.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace gs {

// StringObject Implementation
StringObject::StringObject(const Type& typeRef, const std::string& text)
    : type_(&typeRef), data_(text) {}

const Type& StringObject::getType() const {
    return *type_;
}

std::string& StringObject::data() {
    return data_;
}

const std::string& StringObject::data() const {
    return data_;
}

std::size_t StringObject::size() const {
    return data_.size();
}

char StringObject::at(std::size_t index) const {
    if (index >= data_.size()) {
        throw std::out_of_range("String index out of range");
    }
    return data_[index];
}

bool StringObject::contains(const std::string& substr) const {
    return data_.find(substr) != std::string::npos;
}

std::size_t StringObject::find(const std::string& substr, std::size_t pos) const {
    return data_.find(substr, pos);
}

std::string StringObject::substr(std::size_t pos, std::size_t len) const {
    return data_.substr(pos, len);
}

std::vector<std::string> StringObject::split(const std::string& delimiter) const {
    std::vector<std::string> result;
    if (delimiter.empty()) {
        result.push_back(data_);
        return result;
    }
    
    std::size_t start = 0;
    std::size_t end = data_.find(delimiter);
    
    while (end != std::string::npos) {
        result.push_back(data_.substr(start, end - start));
        start = end + delimiter.length();
        end = data_.find(delimiter, start);
    }
    
    result.push_back(data_.substr(start));
    return result;
}

std::string StringObject::replace(const std::string& from, const std::string& to) const {
    std::string result = data_;
    if (from.empty()) return result;
    
    std::size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

// StringType Implementation
StringType::StringType() {
    registerMethodAttribute("__str__", 0, [this](Object& self,
                                                  const std::vector<Value>& args,
                                                  const StringFactory& makeString,
                                                  const ValueStrInvoker& valueStr) {
        (void)args;
        return makeString(__str__(self, valueStr));
    });

    registerMethod("size", 0, &StringType::methodSize);
    registerMethod("length", 0, &StringType::methodLength);
    registerMethod("contains", 1, &StringType::methodContains);
    registerMethod("find", 1, &StringType::methodFind);
    registerMethod("substr", 2, &StringType::methodSubstr);
    registerMethod("slice", 2, &StringType::methodSlice);
    registerMethod("split", 1, &StringType::methodSplit);
    registerMethod("replace", 2, &StringType::methodReplace);
    registerMethod("upper", 0, &StringType::methodUpper);
    registerMethod("lower", 0, &StringType::methodLower);
    registerMethod("strip", 0, &StringType::methodStrip);
    registerMethod("startsWith", 1, &StringType::methodStartsWith);
    registerMethod("endsWith", 1, &StringType::methodEndsWith);
    registerMethod("at", 1, &StringType::methodAt);

    registerMemberAttribute(
        "length",
        [this](Object& self) { return memberLengthGet(self); },
        [this](Object& self, const Value& value) { return memberLengthSet(self, value); });
}

void StringType::registerMethod(const std::string& name, std::size_t argc, MethodHandler handler) {
    registerMethodAttribute(name,
                            argc,
                            [this, handler](Object& self,
                                            const std::vector<Value>& args,
                                            const StringFactory& makeString,
                                            const ValueStrInvoker& valueStr) {
        return (this->*handler)(self, args, makeString, valueStr);
    });
}

const char* StringType::name() const {
    return "String";
}

Value StringType::callMethod(Object& self,
                           const std::string& method,
                           const std::vector<Value>& args,
                           const StringFactory& makeString,
                           const ValueStrInvoker& valueStr) const {
    return Type::callMethod(self, method, args, makeString, valueStr);
}

Value StringType::getMember(Object& self, const std::string& member) const {
    return Type::getMember(self, member);
}

Value StringType::setMember(Object& self, const std::string& member, const Value& value) const {
    return Type::setMember(self, member, value);
}

std::string StringType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    return str.data();
}

std::string StringType::getStringContent(const Value& value, const ValueStrInvoker& valueStr) {
    if (value.type == ValueType::Ref) {
        Object* obj = value.asRef();
        if (obj && dynamic_cast<StringObject*>(obj)) {
            return static_cast<StringObject*>(obj)->data();
        }
    }
    return valueStr(value);
}

StringObject& StringType::requireString(Object& self) {
    StringObject* str = dynamic_cast<StringObject*>(&self);
    if (!str) {
        throw std::runtime_error("Method called on non-string object");
    }
    return *str;
}

// String method implementations
Value StringType::methodSize(Object& self,
                             const std::vector<Value>& args,
                             const StringFactory& makeString,
                             const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    (void)args;
    (void)makeString;
    (void)valueStr;
    return Value::Int(static_cast<std::int64_t>(str.size()));
}

Value StringType::methodLength(Object& self,
                               const std::vector<Value>& args,
                               const StringFactory& makeString,
                               const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    (void)args;
    (void)makeString;
    (void)valueStr;
    return Value::Int(static_cast<std::int64_t>(str.size()));
}

Value StringType::methodContains(Object& self,
                                 const std::vector<Value>& args,
                                 const StringFactory& makeString,
                                 const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    (void)makeString;
    const std::string substr = getStringContent(args[0], valueStr);
    return Value::Bool(str.contains(substr));
}

Value StringType::methodFind(Object& self,
                             const std::vector<Value>& args,
                             const StringFactory& makeString,
                             const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    (void)makeString;
    const std::string substr = getStringContent(args[0], valueStr);
    std::size_t pos = str.find(substr);
    return pos == std::string::npos ? Value::Int(-1) : Value::Int(static_cast<std::int64_t>(pos));
}

Value StringType::methodSubstr(Object& self,
                               const std::vector<Value>& args,
                               const StringFactory& makeString,
                               const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    (void)valueStr;
    if (args[0].type != ValueType::Int || args[1].type != ValueType::Int) {
        throw std::runtime_error("substr expects integer arguments");
    }
    const std::int64_t startArg = args[0].asInt();
    const std::int64_t lengthArg = args[1].asInt();
    const std::size_t start = startArg <= 0 ? 0 : static_cast<std::size_t>(startArg);
    const std::size_t length = lengthArg <= 0 ? 0 : static_cast<std::size_t>(lengthArg);
    if (start >= str.size()) {
        return makeString("");
    }
    return makeString(str.substr(start, length));
}

Value StringType::methodSlice(Object& self,
                              const std::vector<Value>& args,
                              const StringFactory& makeString,
                              const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    (void)valueStr;
    if (args[0].type != ValueType::Int || args[1].type != ValueType::Int) {
        throw std::runtime_error("slice expects integer arguments");
    }
    const std::int64_t startArg = args[0].asInt();
    const std::int64_t endArg = args[1].asInt();
    const std::size_t size = str.size();
    const std::size_t start = startArg <= 0 ? 0 : std::min<std::size_t>(static_cast<std::size_t>(startArg), size);
    const std::size_t end = endArg <= 0 ? 0 : std::min<std::size_t>(static_cast<std::size_t>(endArg), size);
    if (end <= start) {
        return makeString("");
    }
    return makeString(str.substr(start, end - start));
}

Value StringType::methodSplit(Object& self,
                              const std::vector<Value>& args,
                              const StringFactory& makeString,
                              const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    const std::string delimiter = getStringContent(args[0], valueStr);
    const std::vector<std::string> parts = str.split(delimiter);
    std::ostringstream ss;
    ss << "[";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        ss << parts[i];
    }
    ss << "]";
    return makeString(ss.str());
}

Value StringType::methodReplace(Object& self,
                                const std::vector<Value>& args,
                                const StringFactory& makeString,
                                const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    const std::string from = getStringContent(args[0], valueStr);
    const std::string to = getStringContent(args[1], valueStr);
    return makeString(str.replace(from, to));
}

Value StringType::methodUpper(Object& self,
                              const std::vector<Value>& args,
                              const StringFactory& makeString,
                              const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    (void)args;
    (void)valueStr;
    std::string out = str.data();
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return makeString(out);
}

Value StringType::methodLower(Object& self,
                              const std::vector<Value>& args,
                              const StringFactory& makeString,
                              const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    (void)args;
    (void)valueStr;
    std::string out = str.data();
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return makeString(out);
}

Value StringType::methodStrip(Object& self,
                              const std::vector<Value>& args,
                              const StringFactory& makeString,
                              const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    (void)args;
    (void)valueStr;
    const std::string& src = str.data();
    std::size_t begin = 0;
    while (begin < src.size() && std::isspace(static_cast<unsigned char>(src[begin]))) {
        ++begin;
    }
    std::size_t end = src.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(src[end - 1]))) {
        --end;
    }
    return makeString(src.substr(begin, end - begin));
}

Value StringType::methodStartsWith(Object& self,
                                   const std::vector<Value>& args,
                                   const StringFactory& makeString,
                                   const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    (void)makeString;
    const std::string prefix = getStringContent(args[0], valueStr);
    const std::string& source = str.data();
    if (prefix.size() > source.size()) {
        return Value::Bool(false);
    }
    return Value::Bool(source.compare(0, prefix.size(), prefix) == 0);
}

Value StringType::methodEndsWith(Object& self,
                                 const std::vector<Value>& args,
                                 const StringFactory& makeString,
                                 const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    (void)makeString;
    const std::string suffix = getStringContent(args[0], valueStr);
    const std::string& source = str.data();
    if (suffix.size() > source.size()) {
        return Value::Bool(false);
    }
    return Value::Bool(source.compare(source.size() - suffix.size(), suffix.size(), suffix) == 0);
}

Value StringType::methodAt(Object& self,
                           const std::vector<Value>& args,
                           const StringFactory& makeString,
                           const ValueStrInvoker& valueStr) const {
    StringObject& str = requireString(self);
    (void)valueStr;
    if (args[0].type != ValueType::Int) {
        throw std::runtime_error("at expects an integer index");
    }
    std::size_t index = static_cast<std::size_t>(args[0].asInt());
    char ch = str.at(index);

    return makeString(std::string(1, ch));
}

Value StringType::memberLengthGet(Object& self) const {
    StringObject& str = requireString(self);
    return Value::Int(static_cast<std::int64_t>(str.size()));
}

Value StringType::memberLengthSet(Object& self, const Value& value) const {
    (void)self;
    (void)value;
    throw std::runtime_error("String.length is read-only");
}

} // namespace gs
