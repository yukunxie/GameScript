#include "gs/global.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace gs {

namespace {

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

} // namespace

void bindGlobalModule(HostRegistry& host) {
    host.bind("print", [](HostContext& context, const std::vector<Value>& args) -> Value {
        printValues(context, args, true, true, ", ");
        return Value::Int(0);
    });

    host.bind("printf", [](HostContext& context, const std::vector<Value>& args) -> Value {
        printValues(context, args, false, false, "");
        return Value::Int(0);
    });

    host.bind("str", [](HostContext& context, const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            throw std::runtime_error("str() requires exactly one argument");
        }
        return context.createString(context.__str__(args[0]));
    });

    host.bind("type", [](HostContext& context, const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            throw std::runtime_error("type() requires exactly one argument");
        }
        return context.createString(context.typeName(args[0]));
    });

    host.bind("id", [](HostContext& context, const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            throw std::runtime_error("id() requires exactly one argument");
        }
        const std::uint64_t idValue = context.objectId(args[0]);
        constexpr std::uint64_t kMaxValue = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        if (idValue > kMaxValue) {
            throw std::runtime_error("id() overflow");
        }
        return Value::Int(static_cast<std::int64_t>(idValue));
    });
}

} // namespace gs
