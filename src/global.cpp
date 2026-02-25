#include "gs/global.hpp"
#include "gs/compiler.hpp"
#include "gs/type_system.hpp"

#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace gs {

namespace {

std::string resolveModulePath(const std::string& moduleSpec) {
    namespace fs = std::filesystem;
    std::string normalized = moduleSpec;
    if (normalized.find('/') == std::string::npos && normalized.find('\\') == std::string::npos) {
        for (char& c : normalized) {
            if (c == '.') {
                c = '/';
            }
        }
    }

    std::vector<std::string> candidates;
    candidates.push_back(normalized);
    if (normalized.size() < 3 || normalized.substr(normalized.size() - 3) != ".gs") {
        candidates.push_back(normalized + ".gs");
    }

    const std::vector<fs::path> roots = {
        fs::current_path(),
        fs::current_path() / "scripts",
        fs::current_path().parent_path() / "scripts"
    };

    for (const auto& candidate : candidates) {
        for (const auto& root : roots) {
            fs::path path = root / candidate;
            if (fs::exists(path)) {
                return fs::weakly_canonical(path).string();
            }
        }
    }

    return {};
}

void printValues(HostContext& context,
                 const std::vector<Value>& args,
                 bool withPrefix,
                 bool withTrailingNewline,
                 const char* separator) {
    if (withPrefix) {
        std::cout << "[script]";
    }
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i == 0) {
            std::cout << (withPrefix ? " " : "");
        } else {
            std::cout << separator;
        }
        std::cout << context.__str__(args[i]);
    }
    if (withTrailingNewline) {
        std::cout << '\n';
    }
}

std::string formatAssertMessage(HostContext& context,
                                const std::string& format,
                                const std::vector<Value>& args,
                                std::size_t argStart) {
    std::ostringstream out;
    std::size_t i = 0;
    std::size_t argIndex = argStart;
    while (i < format.size()) {
        if (format[i] == '{' && i + 1 < format.size() && format[i + 1] == '}') {
            if (argIndex < args.size()) {
                out << context.__str__(args[argIndex++]);
            } else {
                out << "{}";
            }
            i += 2;
            continue;
        }
        out << format[i];
        ++i;
    }

    while (argIndex < args.size()) {
        if (out.tellp() > 0) {
            out << ' ';
        }
        out << context.__str__(args[argIndex++]);
    }
    return out.str();
}

const Value& requireFormatArg(const std::vector<Value>& args,
                              std::size_t& argIndex,
                              const char* who) {
    if (argIndex >= args.size()) {
        throw std::runtime_error(std::string(who) + " missing format argument");
    }
    return args[argIndex++];
}

std::int64_t toSignedInt(HostContext& context, const Value& value, const char* who) {
    if (value.isInt()) {
        return value.asInt();
    }
    if (value.isFloat()) {
        return static_cast<std::int64_t>(value.asFloat());
    }
    try {
        return std::stoll(context.__str__(value));
    } catch (...) {
        throw std::runtime_error(std::string(who) + " expected integer argument");
    }
}

std::uint64_t toUnsignedInt(HostContext& context, const Value& value, const char* who) {
    const auto signedValue = toSignedInt(context, value, who);
    return static_cast<std::uint64_t>(signedValue);
}

double toFloatingPoint(HostContext& context, const Value& value, const char* who) {
    if (value.isInt()) {
        return static_cast<double>(value.asInt());
    }
    if (value.isFloat()) {
        return value.asFloat();
    }
    try {
        return std::stod(context.__str__(value));
    } catch (...) {
        throw std::runtime_error(std::string(who) + " expected numeric argument");
    }
}

std::string formatPrintf(const std::string& format,
                         HostContext& context,
                         const std::vector<Value>& args,
                         std::size_t argStart) {
    std::ostringstream out;
    std::size_t argIndex = argStart;
    std::size_t i = 0;

    while (i < format.size()) {
        const char c = format[i];

        if (c == '\\' && i + 1 < format.size()) {
            const char esc = format[i + 1];
            switch (esc) {
            case 'n': out << '\n'; break;
            case 't': out << '\t'; break;
            case 'r': out << '\r'; break;
            case '{': out << '{'; break;
            case '}': out << '}'; break;
            case '"': out << '"'; break;
            case '%': out << '%'; break;
            case '\\': out << '\\'; break;
            default: out << esc; break;
            }
            i += 2;
            continue;
        }

        if (c == '{' && i + 1 < format.size() && format[i + 1] == '}') {
            out << context.__str__(requireFormatArg(args, argIndex, "printf"));
            i += 2;
            continue;
        }

        if (c != '%') {
            out << c;
            ++i;
            continue;
        }

        if (i + 1 < format.size() && format[i + 1] == '%') {
            out << '%';
            i += 2;
            continue;
        }

        ++i;
        bool zeroPad = false;
        int width = 0;
        int precision = -1;

        if (i < format.size() && format[i] == '0') {
            zeroPad = true;
        }
        while (i < format.size() && format[i] >= '0' && format[i] <= '9') {
            width = width * 10 + static_cast<int>(format[i] - '0');
            ++i;
        }

        if (i < format.size() && format[i] == '.') {
            ++i;
            precision = 0;
            bool hasPrecisionDigit = false;
            while (i < format.size() && format[i] >= '0' && format[i] <= '9') {
                precision = precision * 10 + static_cast<int>(format[i] - '0');
                hasPrecisionDigit = true;
                ++i;
            }
            if (!hasPrecisionDigit) {
                throw std::runtime_error("printf invalid precision after '.'");
            }
        }

        if (i >= format.size()) {
            throw std::runtime_error("printf trailing '%' in format string");
        }

        const char spec = format[i++];
        switch (spec) {
        case 'd': {
            const auto& v = requireFormatArg(args, argIndex, "printf");
            std::ostringstream tmp;
            if (width > 0) {
                tmp << std::setw(width) << std::setfill(zeroPad ? '0' : ' ');
            }
            tmp << toSignedInt(context, v, "printf");
            out << tmp.str();
            break;
        }
        case 'u': {
            const auto& v = requireFormatArg(args, argIndex, "printf");
            std::ostringstream tmp;
            if (width > 0) {
                tmp << std::setw(width) << std::setfill(zeroPad ? '0' : ' ');
            }
            tmp << toUnsignedInt(context, v, "printf");
            out << tmp.str();
            break;
        }
        case 'h':
        case 'H': {
            const auto& v = requireFormatArg(args, argIndex, "printf");
            std::ostringstream tmp;
            if (width > 0) {
                tmp << std::setw(width) << std::setfill(zeroPad ? '0' : ' ');
            }
            if (spec == 'H') {
                tmp << std::uppercase;
            }
            tmp << std::hex << toUnsignedInt(context, v, "printf");
            out << tmp.str();
            break;
        }
        case 's': {
            const auto& v = requireFormatArg(args, argIndex, "printf");
            out << context.__str__(v);
            break;
        }
        case 'f': {
            const auto& v = requireFormatArg(args, argIndex, "printf");
            std::ostringstream tmp;
            if (width > 0) {
                tmp << std::setw(width) << std::setfill(zeroPad ? '0' : ' ');
            }
            tmp << std::fixed << std::setprecision(precision >= 0 ? precision : 6);
            tmp << toFloatingPoint(context, v, "printf");
            out << tmp.str();
            break;
        }
        default:
            throw std::runtime_error(std::string("printf unsupported format specifier: %") + spec);
        }
    }

    return out.str();
}

std::vector<std::string> collectRequestedExports(HostContext& context,
                                                 const std::vector<Value>& args,
                                                 std::size_t startIndex) {
    std::vector<std::string> requested;
    if (args.size() <= startIndex) {
        return requested;
    }

    requested.reserve(args.size() - startIndex);
    for (std::size_t i = startIndex; i < args.size(); ++i) {
        const std::string name = context.__str__(args[i]);
        if (name.empty()) {
            throw std::runtime_error("loadModule() export names must be non-empty strings");
        }
        requested.push_back(name);
    }
    return requested;
}

bool scriptModuleContainsExport(const Module& module, const std::string& exportName) {
    for (const auto& global : module.globals) {
        if (global.name == exportName) {
            return true;
        }
    }
    for (const auto& fn : module.functions) {
        if (fn.name == exportName) {
            return true;
        }
    }
    for (const auto& cls : module.classes) {
        if (cls.name == exportName) {
            return true;
        }
    }
    return false;
}

Value resolveModuleExportValue(HostContext& context,
                               const Value& moduleRef,
                               const std::string& moduleName,
                               const std::string& exportName,
                               FunctionType& functionType,
                               ClassType& classType) {
    if (!moduleRef.isRef()) {
        throw std::runtime_error("loadModule() did not return module object for export resolution");
    }

    Object& object = context.getObject(moduleRef);
    auto* moduleObject = dynamic_cast<ModuleObject*>(&object);
    if (!moduleObject) {
        throw std::runtime_error("loadModule() did not return Module object");
    }

    auto exportIt = moduleObject->exports().find(exportName);
    if (exportIt != moduleObject->exports().end()) {
        return exportIt->second;
    }

    const auto& modulePin = moduleObject->modulePin();
    if (modulePin) {
        for (std::size_t i = 0; i < modulePin->functions.size(); ++i) {
            if (modulePin->functions[i].name == exportName) {
                Value out = context.createObject(std::make_unique<FunctionObject>(functionType, i, modulePin));
                moduleObject->exports()[exportName] = out;
                return out;
            }
        }

        for (std::size_t i = 0; i < modulePin->classes.size(); ++i) {
            if (modulePin->classes[i].name == exportName) {
                Value out = context.createObject(std::make_unique<ClassObject>(classType,
                                                                                modulePin->classes[i].name,
                                                                                i,
                                                                                modulePin));
                moduleObject->exports()[exportName] = out;
                return out;
            }
        }

        auto globalIt = moduleObject->exports().find(exportName);
        if (globalIt != moduleObject->exports().end()) {
            return globalIt->second;
        }
    }

    throw std::runtime_error("Module export not found: " + moduleName + "." + exportName);
}

Value buildProjectedModuleObject(HostContext& context,
                                 ModuleType& moduleType,
                                 const std::string& moduleName,
                                 const Value& baseModuleRef,
                                 const std::vector<std::string>& requestedExports,
                                 FunctionType& functionType,
                                 ClassType& classType) {
    Value projectedRef = context.createObject(std::make_unique<ModuleObject>(moduleType, moduleName));
    Object& projectedObject = context.getObject(projectedRef);
    auto* projectedModule = dynamic_cast<ModuleObject*>(&projectedObject);
    if (!projectedModule) {
        throw std::runtime_error("Failed to create projected Module object");
    }

    for (const auto& exportName : requestedExports) {
        projectedModule->exports()[exportName] = resolveModuleExportValue(context,
                                                                          baseModuleRef,
                                                                          moduleName,
                                                                          exportName,
                                                                          functionType,
                                                                          classType);
    }

    return projectedRef;
}

void ensureRequestedExportsAvailable(HostContext& context,
                                     const Value& moduleRef,
                                     const std::string& moduleName,
                                     const std::vector<std::string>& requestedExports) {
    if (requestedExports.empty()) {
        return;
    }

    if (!moduleRef.isRef()) {
        throw std::runtime_error("loadModule() did not return module object for export validation");
    }

    Object& object = context.getObject(moduleRef);
    auto* moduleObject = dynamic_cast<ModuleObject*>(&object);
    if (!moduleObject) {
        throw std::runtime_error("loadModule() did not return Module object");
    }

    const auto& exports = moduleObject->exports();
    const auto& modulePin = moduleObject->modulePin();
    for (const auto& exportName : requestedExports) {
        bool found = exports.contains(exportName);
        if (!found && modulePin) {
            found = scriptModuleContainsExport(*modulePin, exportName);
        }

        if (!found) {
            throw std::runtime_error("Module export not found: " + moduleName + "." + exportName);
        }
    }
}

void resolveRequestedExports(HostContext& context,
                             const Value& moduleRef,
                             const std::string& moduleName,
                             const std::vector<std::string>& requestedExports,
                             FunctionType& functionType,
                             ClassType& classType) {
    for (const auto& exportName : requestedExports) {
        (void)resolveModuleExportValue(context,
                                       moduleRef,
                                       moduleName,
                                       exportName,
                                       functionType,
                                       classType);
    }
}

} // namespace

// ============================================================================
// Module Loading (loadModule function)
// ============================================================================

Value handleLoadModuleBuiltin(HostRegistry& host,
                               HostContext& context,
                               const std::string& moduleName,
                               const std::vector<std::string>& requestedExports,
                               ModuleType& moduleType,
                               NativeFunctionType& nativeFunctionType,
                               FunctionType& functionType,
                               ClassType& classType) {
    const std::string builtinKey = "builtin:" + moduleName;
    Value cached = Value::Nil();
    
    if (context.tryGetCachedModuleObject(builtinKey, cached)) {
        context.ensureModuleInitialized(cached);
        resolveRequestedExports(context, cached, moduleName, requestedExports, functionType, classType);
        
        if (requestedExports.size() == 1) {
            return resolveModuleExportValue(context, cached, moduleName, requestedExports[0], functionType, classType);
        }
        if (requestedExports.size() > 1) {
            return buildProjectedModuleObject(context, moduleType, moduleName, cached, requestedExports, functionType, classType);
        }
        return cached;
    }

    Value moduleRef = host.resolveBuiltin(moduleName, context, nativeFunctionType, moduleType);
    context.cacheModuleObject(builtinKey, moduleRef);
    context.ensureModuleInitialized(moduleRef);
    resolveRequestedExports(context, moduleRef, moduleName, requestedExports, functionType, classType);
    
    if (requestedExports.size() == 1) {
        return resolveModuleExportValue(context, moduleRef, moduleName, requestedExports[0], functionType, classType);
    }
    if (requestedExports.size() > 1) {
        return buildProjectedModuleObject(context, moduleType, moduleName, moduleRef, requestedExports, functionType, classType);
    }
    return moduleRef;
}

Value handleLoadModuleFile(HostContext& context,
                            const std::string& moduleName,
                            const std::string& modulePath,
                            const std::vector<std::string>& requestedExports,
                            ModuleType& moduleType,
                            FunctionType& functionType,
                            ClassType& classType) {
    static std::unordered_map<std::string, std::shared_ptr<Module>> moduleCache;
    
    auto it = moduleCache.find(modulePath);
    if (it == moduleCache.end()) {
        auto compiled = std::make_shared<Module>(compileSourceFile(modulePath));
        it = moduleCache.emplace(modulePath, std::move(compiled)).first;
    }

    const std::string moduleKey = "file:" + modulePath;
    Value cached = Value::Nil();
    
    if (context.tryGetCachedModuleObject(moduleKey, cached)) {
        context.ensureModuleInitialized(cached);
        resolveRequestedExports(context, cached, moduleName, requestedExports, functionType, classType);
        
        if (requestedExports.size() == 1) {
            return resolveModuleExportValue(context, cached, moduleName, requestedExports[0], functionType, classType);
        }
        if (requestedExports.size() > 1) {
            return buildProjectedModuleObject(context, moduleType, moduleName, cached, requestedExports, functionType, classType);
        }
        return cached;
    }

    Value moduleRef = context.createObject(std::make_unique<ModuleObject>(moduleType, moduleName, it->second));
    context.cacheModuleObject(moduleKey, moduleRef);
    context.ensureModuleInitialized(moduleRef);
    resolveRequestedExports(context, moduleRef, moduleName, requestedExports, functionType, classType);
    
    if (requestedExports.size() == 1) {
        return resolveModuleExportValue(context, moduleRef, moduleName, requestedExports[0], functionType, classType);
    }
    if (requestedExports.size() > 1) {
        return buildProjectedModuleObject(context, moduleType, moduleName, moduleRef, requestedExports, functionType, classType);
    }
    return moduleRef;
}

// ============================================================================
// Global Functions Implementation
// ============================================================================

Value impl_loadModule(HostRegistry& host, HostContext& context, const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("loadModule() accepts at least one argument");
    }
    
    const std::string moduleName = context.__str__(args[0]);
    const std::vector<std::string> requestedExports = collectRequestedExports(context, args, 1);

    static ModuleType moduleType;
    static NativeFunctionType nativeFunctionType;
    static FunctionType functionType;
    static ClassType classType;

    // Try builtin module first
    if (host.hasModule(moduleName)) {
        return handleLoadModuleBuiltin(host, context, moduleName, requestedExports,
                                       moduleType, nativeFunctionType, functionType, classType);
    }

    // Try file-based module
    const std::string modulePath = resolveModulePath(moduleName);
    if (modulePath.empty()) {
        throw std::runtime_error("Module not found: " + moduleName);
    }

    return handleLoadModuleFile(context, moduleName, modulePath, requestedExports,
                                moduleType, functionType, classType);
}

Value impl_print(HostContext& context, const std::vector<Value>& args) {
    printValues(context, args, true, true, " ");
    return Value::Int(0);
}

Value impl_printf(HostContext& context, const std::vector<Value>& args) {
    if (args.empty()) {
        return Value::Int(0);
    }
    const std::string fmt = context.__str__(args[0]);
    std::cout << formatPrintf(fmt, context, args, 1);
    return Value::Int(0);
}

Value impl_str(HostContext& context, const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("str() requires exactly one argument");
    }
    return context.createString(context.__str__(args[0]));
}

Value impl_Tuple(HostContext& context, const std::vector<Value>& args) {
    static TupleType tupleType;
    std::vector<Value> values;
    values.reserve(args.size());
    for (const auto& value : args) {
        values.push_back(value);
    }
    return context.createObject(std::make_unique<TupleObject>(tupleType, std::move(values)));
}

Value impl_type(HostContext& context, const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("type() requires exactly one argument");
    }
    return context.createString(context.typeName(args[0]));
}

Value impl_id(HostContext& context, const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("id() requires exactly one argument");
    }
    const std::uint64_t idValue = context.objectId(args[0]);
    constexpr std::uint64_t kMaxValue = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (idValue > kMaxValue) {
        throw std::runtime_error("id() overflow");
    }
    return Value::Int(static_cast<std::int64_t>(idValue));
}

Value impl_assert(HostContext& context, const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("assert(condition, format, args...) requires at least condition");
    }

    // Evaluate condition
    bool condition = false;
    if (args[0].isInt()) {
        condition = args[0].asInt() != 0;
    } else if (args[0].isFloat()) {
        condition = std::abs(args[0].asFloat()) > std::numeric_limits<double>::epsilon();
    } else if (args[0].isNil()) {
        condition = false;
    } else {
        condition = true;
    }
    
    if (condition) {
        return Value::Int(1);
    }

    // Build error message
    std::string message = "assert failed";
    if (args.size() >= 2) {
        message = formatAssertMessage(context, context.__str__(args[1]), args, 2);
    }
    throw std::runtime_error("assertion failed: " + message);
}

// ============================================================================
// Binding Registration
// ============================================================================

void bindGlobalModule(HostRegistry& host) {
    // Note: These functions use variadic arguments and HostContext access,
    // which requires the legacy binding API. The new BindingContext is designed
    // for simpler C++ functions and classes with fixed signatures.
    
    host.bind("loadModule", [&host](HostContext& ctx, const std::vector<Value>& args) -> Value {
        return impl_loadModule(host, ctx, args);
    });

    host.bind("print", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        return impl_print(ctx, args);
    });

    host.bind("printf", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        return impl_printf(ctx, args);
    });

    host.bind("str", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        return impl_str(ctx, args);
    });

    host.bind("Tuple", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        return impl_Tuple(ctx, args);
    });

    host.bind("type", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        return impl_type(ctx, args);
    });

    host.bind("id", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        return impl_id(ctx, args);
    });

    host.bind("assert", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        return impl_assert(ctx, args);
    });
}

} // namespace gs
