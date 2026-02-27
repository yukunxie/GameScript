#pragma once

#include "gs/type_system.hpp"
#include <regex>
#include <memory>
#include <string>
#include <vector>

namespace gs {

class HostContext;

// ============================================================================
// MatchObject - 表示正则表达式匹配结果
// ============================================================================

class MatchObject : public Object {
public:
    MatchObject(const Type& typeRef, std::size_t start, std::size_t end, const std::string& matched);
    
    const Type& getType() const override;
    
    std::size_t start() const { return start_; }
    std::size_t end() const { return end_; }
    const std::string& matched() const { return matched_; }

private:
    const Type* type_;
    std::size_t start_;
    std::size_t end_;
    std::string matched_;
};

class MatchType : public Type {
public:
    MatchType();
    const char* name() const override { return "Match"; }
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;

private:
    MatchObject& requireMatch(Object& self) const;
    void initializeSlots();
};

// ============================================================================
// PatternObject - 表示编译后的正则表达式
// ============================================================================

class PatternObject : public Object {
public:
    PatternObject(const Type& typeRef, const std::string& pattern, int flags = 0);
    
    const Type& getType() const override;
    
    // 搜索功能
    bool search(const std::string& text) const;
    std::unique_ptr<MatchObject> match(const std::string& text, const Type& matchType) const;
    std::vector<std::unique_ptr<MatchObject>> matchAll(const std::string& text, const Type& matchType) const;
    
    const std::string& pattern() const { return pattern_; }

private:
    const Type* type_;
    std::string pattern_;
    std::regex regex_;
    int flags_;
};

class PatternType : public Type {
public:
    PatternType();
    const char* name() const override { return "Pattern"; }
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
    
    // Thread-local context management (similar to BoundClassType)
    static void setThreadLocalContext(HostContext* ctx);
    static HostContext* getThreadLocalContext();

private:
    PatternObject& requirePattern(Object& self) const;
    void initializeSlots();
    
    // Thread-local storage for HostContext
    static thread_local HostContext* threadContext_;
};

} // namespace gs