#include "gs/string_module.hpp"
#include "gs/type_system/regex_type.hpp"
#include "gs/type_system/native_function_type.hpp"
#include "gs/type_system.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace gs {

// 全局类型实例
MatchType g_matchType; // 移除 static 以便外部访问
static PatternType g_patternType;

// ============================================================================
// String 格式化函数 (类似 printf)
// ============================================================================

std::string formatStringPrintf(const std::string& format, 
                              HostContext& context,
                              const std::vector<Value>& args,
                              std::size_t argStart = 0) {
    std::ostringstream out;
    std::size_t argIndex = argStart;
    std::size_t i = 0;

    while (i < format.size()) {
        const char c = format[i];

        // 处理转义序列
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

        // 处理 {} 格式化
        if (c == '{' && i + 1 < format.size() && format[i + 1] == '}') {
            if (argIndex < args.size()) {
                out << context.__str__(args[argIndex++]);
            } else {
                out << "{}";
            }
            i += 2;
            continue;
        }

        // 处理 % 格式化
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

        // 解析标志
        if (i < format.size() && format[i] == '0') {
            zeroPad = true;
            ++i;
        }

        // 解析宽度
        while (i < format.size() && format[i] >= '0' && format[i] <= '9') {
            width = width * 10 + static_cast<int>(format[i] - '0');
            ++i;
        }

        // 解析精度
        if (i < format.size() && format[i] == '.') {
            ++i;
            precision = 0;
            while (i < format.size() && format[i] >= '0' && format[i] <= '9') {
                precision = precision * 10 + static_cast<int>(format[i] - '0');
                ++i;
            }
        }

        if (i >= format.size()) {
            throw std::runtime_error("format: trailing '%' in format string");
        }

        const char spec = format[i++];
        
        if (argIndex >= args.size()) {
            throw std::runtime_error("format: not enough arguments for format string");
        }

        const Value& arg = args[argIndex++];

        switch (spec) {
        case 'd': {
            std::int64_t value = arg.isInt() ? arg.asInt() : 
                               arg.isFloat() ? static_cast<std::int64_t>(arg.asFloat()) :
                               std::stoll(context.__str__(arg));
            std::ostringstream tmp;
            if (width > 0) {
                tmp << std::setw(width) << std::setfill(zeroPad ? '0' : ' ');
            }
            tmp << value;
            out << tmp.str();
            break;
        }
        case 'u': {
            std::uint64_t value = arg.isInt() ? static_cast<std::uint64_t>(arg.asInt()) : 
                                arg.isFloat() ? static_cast<std::uint64_t>(arg.asFloat()) :
                                std::stoull(context.__str__(arg));
            std::ostringstream tmp;
            if (width > 0) {
                tmp << std::setw(width) << std::setfill(zeroPad ? '0' : ' ');
            }
            tmp << value;
            out << tmp.str();
            break;
        }
        case 'h':
        case 'H': {
            std::uint64_t value = arg.isInt() ? static_cast<std::uint64_t>(arg.asInt()) : 
                                arg.isFloat() ? static_cast<std::uint64_t>(arg.asFloat()) :
                                std::stoull(context.__str__(arg));
            std::ostringstream tmp;
            if (width > 0) {
                tmp << std::setw(width) << std::setfill(zeroPad ? '0' : ' ');
            }
            if (spec == 'H') {
                tmp << std::uppercase;
            }
            tmp << std::hex << value;
            out << tmp.str();
            break;
        }
        case 's': {
            out << context.__str__(arg);
            break;
        }
        case 'f': {
            double value = arg.isFloat() ? arg.asFloat() : 
                          arg.isInt() ? static_cast<double>(arg.asInt()) :
                          std::stod(context.__str__(arg));
            std::ostringstream tmp;
            if (width > 0) {
                tmp << std::setw(width) << std::setfill(zeroPad ? '0' : ' ');
            }
            tmp << std::fixed << std::setprecision(precision >= 0 ? precision : 6);
            tmp << value;
            out << tmp.str();
            break;
        }
        default:
            throw std::runtime_error("format: unsupported format specifier: %" + std::string(1, spec));
        }
    }

    return out.str();
}

// ============================================================================
// String Module Functions
// ============================================================================

Value impl_string_format(HostContext& context, const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("string.format() requires at least 1 argument");
    }
    
    const std::string format = context.__str__(args[0]);
    const std::string result = formatStringPrintf(format, context, args, 1);
    return context.createString(result);
}

Value impl_string_compile(HostContext& context, const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("string.compile() requires at least 1 argument");
    }
    
    const std::string pattern = context.__str__(args[0]);
    int flags = 0;
    if (args.size() > 1 && args[1].isInt()) {
        flags = static_cast<int>(args[1].asInt());
    }
    
    try {
        return context.createObject(std::make_unique<PatternObject>(g_patternType, pattern, flags));
    } catch (const std::runtime_error& e) {
        throw std::runtime_error("string.compile(): " + std::string(e.what()));
    }
}

// Note: Pattern 对象的 match() 和 matchAll() 方法现在已经直接在 
// PatternType 中实现，使用线程本地存储的 HostContext

// ============================================================================
// 模块注册
// ============================================================================

void registerStringModule(HostRegistry& host) {
    // 注册 string 模块函数
    host.bindModuleFunction("string", "format", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        return impl_string_format(ctx, args);
    });
    
    host.bindModuleFunction("string", "compile", [](HostContext& ctx, const std::vector<Value>& args) -> Value {
        return impl_string_compile(ctx, args);
    });
    
    // Note: pattern.match() and pattern.matchAll() are now directly implemented 
    // in PatternType using thread-local HostContext, so we no longer need
    // the global helper functions __pattern_match and __pattern_matchAll
    
    std::cout << "[C++] String module with regex support registered successfully" << std::endl;
}

} // namespace gs