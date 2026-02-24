#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace gs {

enum class TokenType {
    End,
    Identifier,
    Number,
    String,

    KeywordFn,
    KeywordClass,
    KeywordExtends,
    KeywordLet,
    KeywordFor,
    KeywordIn,
    KeywordIf,
    KeywordElif,
    KeywordElse,
    KeywordWhile,
    KeywordBreak,
    KeywordContinue,
    KeywordStr,
    KeywordReturn,
    KeywordSpawn,
    KeywordAwait,
    KeywordSleep,
    KeywordYield,

    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Dot,
    Comma,
    Colon,
    Semicolon,
    Bang,
    BangEqual,
    FatArrow,
    Equal,
    EqualEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Plus,
    Minus,
    Star,
    Slash
};

struct Token {
    TokenType type{TokenType::End};
    std::string text;
    std::size_t line{1};
    std::size_t column{1};
};

class Tokenizer {
public:
    explicit Tokenizer(std::string source);
    std::vector<Token> tokenize();

private:
    char peek() const;
    char peekNext() const;
    char advance();
    bool isAtEnd() const;
    void skipWhitespace();
    Token makeToken(TokenType type, const std::string& text) const;
    Token identifierOrKeyword();
    Token number();
    Token stringLiteral();

    std::string source_;
    std::size_t index_{0};
    std::size_t line_{1};
    std::size_t column_{1};
};

} // namespace gs
