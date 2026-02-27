#include "gs/type_system/regex_type.hpp"
#include "gs/type_system/native_function_type.hpp"
#include "gs/type_system/list_type.hpp"
#include "gs/binding.hpp"
#include "gs/bytecode.hpp"
#include <iostream>
#include <stdexcept>

namespace gs {

// 外部声明的全局类型实例 (从 string_module.cpp)
extern MatchType g_matchType;

// ============================================================================
// MatchObject 实现
// ============================================================================

MatchObject::MatchObject(const Type& typeRef, std::size_t start, std::size_t end, const std::string& matched)
    : type_(&typeRef), start_(start), end_(end), matched_(matched) {
}

const Type& MatchObject::getType() const {
    return *type_;
}

// ============================================================================
// MatchType 实现
// ============================================================================

MatchType::MatchType() {
    initializeSlots();
}

std::string MatchType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto& match = requireMatch(self);
    return "<Match start=" + std::to_string(match.start()) + 
           " end=" + std::to_string(match.end()) + 
           " matched='" + match.matched() + "'>";
}

MatchObject& MatchType::requireMatch(Object& self) const {
    auto* match = dynamic_cast<MatchObject*>(&self);
    if (!match) {
        throw std::runtime_error("Expected MatchObject, got " + std::string(self.getType().name()));
    }
    return *match;
}

void MatchType::initializeSlots() {
    registerMethodAttribute("start", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return Value::Int(static_cast<std::int64_t>(requireMatch(self).start()));
    });
    
    registerMethodAttribute("end", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return Value::Int(static_cast<std::int64_t>(requireMatch(self).end()));
    });
    
    registerMethodAttribute("matched", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return makeString(requireMatch(self).matched());
    });
    
    registerMethodAttribute("group", 0, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        return makeString(requireMatch(self).matched());
    });
}

// ============================================================================
// PatternObject 实现  
// ============================================================================

PatternObject::PatternObject(const Type& typeRef, const std::string& pattern, int flags)
    : type_(&typeRef), pattern_(pattern), flags_(flags) {
    
    std::regex_constants::syntax_option_type regexFlags = std::regex_constants::ECMAScript;
    
    // 处理标志位 (简化版本，可以后续扩展)
    if (flags & 1) { // case insensitive
        regexFlags |= std::regex_constants::icase;
    }
    
    try {
        regex_ = std::regex(pattern, regexFlags);
    } catch (const std::regex_error& e) {
        throw std::runtime_error("Invalid regex pattern: " + pattern + " (" + e.what() + ")");
    }
}

const Type& PatternObject::getType() const {
    return *type_;
}

bool PatternObject::search(const std::string& text) const {
    return std::regex_search(text, regex_);
}

std::unique_ptr<MatchObject> PatternObject::match(const std::string& text, const Type& matchType) const {
    std::smatch match;
    if (std::regex_search(text, match, regex_)) {
        return std::make_unique<MatchObject>(matchType, 
                                           static_cast<std::size_t>(match.position()),
                                           static_cast<std::size_t>(match.position() + match.length()),
                                           match.str());
    }
    return nullptr;
}

std::vector<std::unique_ptr<MatchObject>> PatternObject::matchAll(const std::string& text, const Type& matchType) const {
    std::vector<std::unique_ptr<MatchObject>> results;
    std::sregex_iterator iter(text.begin(), text.end(), regex_);
    std::sregex_iterator end;
    
    while (iter != end) {
        const std::smatch& match = *iter;
        results.push_back(std::make_unique<MatchObject>(matchType,
                                                       static_cast<std::size_t>(match.position()),
                                                       static_cast<std::size_t>(match.position() + match.length()),
                                                       match.str()));
        ++iter;
    }
    
    return results;
}

// ============================================================================
// PatternType 实现
// ============================================================================

// Initialize thread-local storage
thread_local HostContext* PatternType::threadContext_ = nullptr;

PatternType::PatternType() {
    initializeSlots();
}

std::string PatternType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto& pattern = requirePattern(self);
    return "<Pattern '" + pattern.pattern() + "'>";
}

PatternObject& PatternType::requirePattern(Object& self) const {
    auto* pattern = dynamic_cast<PatternObject*>(&self);
    if (!pattern) {
        throw std::runtime_error("Expected PatternObject, got " + std::string(self.getType().name()));
    }
    return *pattern;
}

void PatternType::initializeSlots() {
    registerMethodAttribute("search", 1, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        if (args.empty()) {
            throw std::runtime_error("search() requires 1 argument");
        }
        std::string text = valueStr(args[0]);
        return Value::Int(requirePattern(self).search(text) ? 1 : 0);
    });
    
    registerMethodAttribute("match", 1, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        if (args.empty()) {
            throw std::runtime_error("match() requires 1 argument");
        }
        
        HostContext* ctx = getThreadLocalContext();
        if (!ctx) {
            throw std::runtime_error("HostContext not available for pattern.match()");
        }
        
        const std::string text = valueStr(args[0]);
        auto matchResult = requirePattern(self).match(text, g_matchType);
        
        if (matchResult) {
            return ctx->createObject(std::make_unique<MatchObject>(
                g_matchType,
                matchResult->start(),
                matchResult->end(),
                matchResult->matched()
            ));
        }
        return Value::Nil();
    });
    
    registerMethodAttribute("matchAll", 1, [this](Object& self, const std::vector<Value>& args, const StringFactory& makeString, const ValueStrInvoker& valueStr) -> Value {
        if (args.empty()) {
            throw std::runtime_error("matchAll() requires 1 argument");
        }
        
        HostContext* ctx = getThreadLocalContext();
        if (!ctx) {
            throw std::runtime_error("HostContext not available for pattern.matchAll()");
        }
        
        const std::string text = valueStr(args[0]);
        auto matches = requirePattern(self).matchAll(text, g_matchType);
        
        // 创建 List 对象包含所有匹配结果
        static ListType listType;
        std::vector<Value> matchValues;
        matchValues.reserve(matches.size());
        
        for (auto& match : matches) {
            matchValues.push_back(ctx->createObject(std::make_unique<MatchObject>(
                g_matchType,
                match->start(),
                match->end(),
                match->matched()
            )));
        }
        
        return ctx->createObject(std::make_unique<ListObject>(listType, std::move(matchValues)));
    });
    
    registerMemberAttribute("pattern", 
        [this](Object& self) -> Value { 
            return Value::Nil(); // 需要 StringFactory
        },
        [this](Object& self, const Value& value) -> Value {
            throw std::runtime_error("pattern is read-only");
        });
}

void PatternType::setThreadLocalContext(HostContext* ctx) {
    threadContext_ = ctx;
}

HostContext* PatternType::getThreadLocalContext() {
    return threadContext_;
}

} // namespace gs
