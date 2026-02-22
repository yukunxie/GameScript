#include "gs/tokenizer.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace gs {

namespace {

std::string formatTokenizerError(const std::string& message, std::size_t line, std::size_t column) {
    std::ostringstream out;
    out << line << ":" << column << ": error: " << message << " [function: <module>]";
    return out.str();
}

} // namespace

Tokenizer::Tokenizer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        skipWhitespace();
        if (isAtEnd()) {
            break;
        }

        const char c = peek();
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(identifierOrKeyword());
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(number());
            continue;
        }
        if (c == '"') {
            tokens.push_back(stringLiteral());
            continue;
        }

        const std::size_t line = line_;
        const std::size_t column = column_;
        switch (advance()) {
        case '(':
            tokens.push_back({TokenType::LParen, "(", line, column});
            break;
        case ')':
            tokens.push_back({TokenType::RParen, ")", line, column});
            break;
        case '{':
            tokens.push_back({TokenType::LBrace, "{", line, column});
            break;
        case '}':
            tokens.push_back({TokenType::RBrace, "}", line, column});
            break;
        case '[':
            tokens.push_back({TokenType::LBracket, "[", line, column});
            break;
        case ']':
            tokens.push_back({TokenType::RBracket, "]", line, column});
            break;
        case '.':
            tokens.push_back({TokenType::Dot, ".", line, column});
            break;
        case ',':
            tokens.push_back({TokenType::Comma, ",", line, column});
            break;
        case ':':
            tokens.push_back({TokenType::Colon, ":", line, column});
            break;
        case '!':
            if (!isAtEnd() && peek() == '=') {
                advance();
                tokens.push_back({TokenType::BangEqual, "!=", line, column});
            } else {
                tokens.push_back({TokenType::Bang, "!", line, column});
            }
            break;
        case ';':
            tokens.push_back({TokenType::Semicolon, ";", line, column});
            break;
        case '=':
            if (!isAtEnd() && peek() == '=') {
                advance();
                tokens.push_back({TokenType::EqualEqual, "==", line, column});
            } else {
                tokens.push_back({TokenType::Equal, "=", line, column});
            }
            break;
        case '<':
            if (!isAtEnd() && peek() == '=') {
                advance();
                tokens.push_back({TokenType::LessEqual, "<=", line, column});
            } else {
                tokens.push_back({TokenType::Less, "<", line, column});
            }
            break;
        case '>':
            if (!isAtEnd() && peek() == '=') {
                advance();
                tokens.push_back({TokenType::GreaterEqual, ">=", line, column});
            } else {
                tokens.push_back({TokenType::Greater, ">", line, column});
            }
            break;
        case '+':
            tokens.push_back({TokenType::Plus, "+", line, column});
            break;
        case '-':
            tokens.push_back({TokenType::Minus, "-", line, column});
            break;
        case '*':
            tokens.push_back({TokenType::Star, "*", line, column});
            break;
        case '/':
            tokens.push_back({TokenType::Slash, "/", line, column});
            break;
        default:
            throw std::runtime_error(formatTokenizerError("Unexpected character in script source", line, column));
        }
    }

    tokens.push_back({TokenType::End, "", line_, column_});
    return tokens;
}

char Tokenizer::peek() const {
    return source_[index_];
}

char Tokenizer::peekNext() const {
    if (index_ + 1 >= source_.size()) {
        return '\0';
    }
    return source_[index_ + 1];
}

char Tokenizer::advance() {
    const char c = source_[index_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Tokenizer::isAtEnd() const {
    return index_ >= source_.size();
}

void Tokenizer::skipWhitespace() {
    while (!isAtEnd()) {
        const char c = peek();
        if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
            advance();
            continue;
        }
        if (c == '#') {
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
            continue;
        }
        if (c == '/' && peekNext() == '/') {
            advance();
            advance();
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
            continue;
        }
        if (c == '/' && peekNext() == '*') {
            const std::size_t startLine = line_;
            const std::size_t startColumn = column_;
            advance();
            advance();
            bool closed = false;
            while (!isAtEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                throw std::runtime_error(formatTokenizerError("Unterminated block comment", startLine, startColumn));
            }
            continue;
        }
        break;
    }
}

Token Tokenizer::makeToken(TokenType type, const std::string& text) const {
    return {type, text, line_, column_};
}

Token Tokenizer::identifierOrKeyword() {
    const std::size_t start = index_;
    const std::size_t line = line_;
    const std::size_t col = column_;
    while (!isAtEnd()) {
        const char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            advance();
        } else {
            break;
        }
    }

    const std::string text = source_.substr(start, index_ - start);
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"fn", TokenType::KeywordFn},       {"class", TokenType::KeywordClass},
        {"extends", TokenType::KeywordExtends},
        {"let", TokenType::KeywordLet},
        {"for", TokenType::KeywordFor},     {"in", TokenType::KeywordIn},
        {"if", TokenType::KeywordIf},       {"elif", TokenType::KeywordElif},
        {"else", TokenType::KeywordElse},   {"while", TokenType::KeywordWhile},
        {"break", TokenType::KeywordBreak}, {"continue", TokenType::KeywordContinue},
        {"str", TokenType::KeywordStr},
        {"return", TokenType::KeywordReturn},
        {"spawn", TokenType::KeywordSpawn},
        {"await", TokenType::KeywordAwait}, {"sleep", TokenType::KeywordSleep},
        {"yield", TokenType::KeywordYield},
    };

    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return {it->second, text, line, col};
    }
    return {TokenType::Identifier, text, line, col};
}

Token Tokenizer::number() {
    const std::size_t start = index_;
    const std::size_t line = line_;
    const std::size_t col = column_;
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    if (!isAtEnd() && peek() == '.') {
        const std::size_t dotPos = index_;
        advance();
        if (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        } else {
            index_ = dotPos;
            --column_;
        }
    }
    return {TokenType::Number, source_.substr(start, index_ - start), line, col};
}

Token Tokenizer::stringLiteral() {
    const std::size_t line = line_;
    const std::size_t col = column_;
    advance();

    std::string value;
    while (!isAtEnd()) {
        const char c = advance();
        if (c == '"') {
            return {TokenType::String, value, line, col};
        }
        if (c == '\\') {
            if (isAtEnd()) {
                throw std::runtime_error(formatTokenizerError("Unterminated escape sequence in string literal", line_, column_));
            }
            const char esc = advance();
            switch (esc) {
            case 'n': value.push_back('\n'); break;
            case 't': value.push_back('\t'); break;
            case 'r': value.push_back('\r'); break;
            case '\\': value.push_back('\\'); break;
            case '"': value.push_back('"'); break;
            default: value.push_back(esc); break;
            }
            continue;
        }
        value.push_back(c);
    }

    throw std::runtime_error(formatTokenizerError("Unterminated string literal", line, col));
}

} // namespace gs
