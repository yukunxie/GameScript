#include "gs/parser.hpp"

#include <sstream>
#include <stdexcept>

namespace gs {

namespace {

Value parseNumericLiteral(const std::string& text) {
    if (text.find('.') != std::string::npos) {
        return Value::Float(std::stod(text));
    }
    return Value::Int(static_cast<std::int64_t>(std::stoll(text)));
}

} // namespace

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

Program Parser::parseProgram() {
    Program program;
    while (!isAtEnd()) {
        if (check(TokenType::KeywordClass)) {
            program.classes.push_back(parseClass());
        } else if (check(TokenType::KeywordFn)) {
            program.functions.push_back(parseFunction());
        } else {
            Stmt stmt = parseStatement();
            program.topLevelStatements.push_back(std::move(stmt));
        }
    }
    return program;
}

std::string Parser::currentScopeName() const {
    if (!currentClassName_.empty() && !currentFunctionName_.empty()) {
        return currentClassName_ + "::" + currentFunctionName_;
    }
    if (!currentFunctionName_.empty()) {
        return currentFunctionName_;
    }
    return "<module>";
}

std::string Parser::formatParseError(const char* message, const Token& token) const {
    std::ostringstream out;
    out << token.line
        << ":" << token.column
        << ": error: " << message
        << " [function: " << currentScopeName() << "]";
    return out.str();
}

const Token& Parser::peek() const {
    return tokens_[current_];
}

const Token& Parser::previous() const {
    return tokens_[current_ - 1];
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::End;
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) {
        return false;
    }
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        ++current_;
        return true;
    }
    return false;
}

const Token& Parser::consume(TokenType type, const char* message) {
    if (check(type)) {
        ++current_;
        return previous();
    }

    const Token* errorToken = &peek();
    if (type == TokenType::Semicolon && current_ > 0) {
        errorToken = &previous();
    }
    throw std::runtime_error(formatParseError(message, *errorToken));
}

ClassDecl Parser::parseClass() {
    consume(TokenType::KeywordClass, "Expected 'class'");
    ClassDecl cls;
    const Token& classNameToken = consume(TokenType::Identifier, "Expected class name");
    cls.name = classNameToken.text;
    cls.line = classNameToken.line;
    cls.column = classNameToken.column;
    if (match(TokenType::KeywordExtends)) {
        cls.baseName = consume(TokenType::Identifier, "Expected base class name").text;
    }
    consume(TokenType::LBrace, "Expected '{' after class name");
    const std::string previousClassName = currentClassName_;
    currentClassName_ = cls.name;
    while (!check(TokenType::RBrace) && !isAtEnd()) {
        if (check(TokenType::KeywordFn)) {
            cls.methods.push_back(parseFunction());
        } else {
            ClassAttrDecl attr;
            const Token& attrToken = consume(TokenType::Identifier, "Expected attribute name");
            attr.name = attrToken.text;
            attr.line = attrToken.line;
            attr.column = attrToken.column;
            consume(TokenType::Equal, "Expected '=' after attribute name");
            attr.initializer = parseExpression();
            consume(TokenType::Semicolon, "Expected ';' after attribute declaration");
            cls.attributes.push_back(std::move(attr));
        }
    }
    currentClassName_ = previousClassName;
    consume(TokenType::RBrace, "Expected '}' after class body");
    return cls;
}

FunctionDecl Parser::parseFunction() {
    consume(TokenType::KeywordFn, "Expected 'fn'");
    FunctionDecl fn;
    const Token& fnNameToken = consume(TokenType::Identifier, "Expected function name");
    fn.name = fnNameToken.text;
    fn.line = fnNameToken.line;
    fn.column = fnNameToken.column;
    const std::string previousFunctionName = currentFunctionName_;
    currentFunctionName_ = fn.name;
    consume(TokenType::LParen, "Expected '('");

    if (!check(TokenType::RParen)) {
        do {
            fn.params.push_back(consume(TokenType::Identifier, "Expected parameter name").text);
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RParen, "Expected ')'");
    consume(TokenType::LBrace, "Expected '{'");
    fn.body = parseBlock();
    currentFunctionName_ = previousFunctionName;
    return fn;
}

std::vector<Stmt> Parser::parseBlock() {
    std::vector<Stmt> body;
    while (!check(TokenType::RBrace) && !isAtEnd()) {
        body.push_back(parseStatement());
    }
    consume(TokenType::RBrace, "Expected '}'");
    return body;
}

CallData Parser::parseCallData() {
    CallData call;
    call.callee = consume(TokenType::Identifier, "Expected callee name").text;
    consume(TokenType::LParen, "Expected '('");
    if (!check(TokenType::RParen)) {
        do {
            call.args.push_back(parseExpression());
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RParen, "Expected ')'");
    return call;
}

Expr Parser::parsePostfix(Expr expr) {
    for (;;) {
        if (match(TokenType::LParen)) {
            Expr call;
            call.type = ExprType::Call;
            call.line = expr.line;
            call.column = expr.column;
            call.callee = std::make_unique<Expr>(std::move(expr));
            if (!check(TokenType::RParen)) {
                do {
                    call.args.push_back(parseExpression());
                } while (match(TokenType::Comma));
            }
            consume(TokenType::RParen, "Expected ')' after call arguments");
            expr = std::move(call);
            continue;
        }

        if (match(TokenType::Dot)) {
            const std::string memberName = consume(TokenType::Identifier, "Expected member name after '.'").text;
            if (match(TokenType::LParen)) {
                Expr methodCall;
                methodCall.type = ExprType::MethodCall;
                methodCall.line = expr.line;
                methodCall.column = expr.column;
                methodCall.object = std::make_unique<Expr>(std::move(expr));
                methodCall.methodName = memberName;
                if (!check(TokenType::RParen)) {
                    do {
                        methodCall.args.push_back(parseExpression());
                    } while (match(TokenType::Comma));
                }
                consume(TokenType::RParen, "Expected ')' after method arguments");
                expr = std::move(methodCall);
                continue;
            }

            Expr prop;
            prop.type = ExprType::PropertyAccess;
            prop.line = expr.line;
            prop.column = expr.column;
            prop.object = std::make_unique<Expr>(std::move(expr));
            prop.propertyName = memberName;
            expr = std::move(prop);
            continue;
        }

        if (match(TokenType::LBracket)) {
            Expr indexAccess;
            indexAccess.type = ExprType::IndexAccess;
            indexAccess.line = expr.line;
            indexAccess.column = expr.column;
            indexAccess.object = std::make_unique<Expr>(std::move(expr));
            indexAccess.index = std::make_unique<Expr>(parseExpression());
            consume(TokenType::RBracket, "Expected ']' after index expression");
            expr = std::move(indexAccess);
            continue;
        }

        break;
    }

    return expr;
}

Stmt Parser::parseForStatement() {
    const Token& forToken = previous();
    consume(TokenType::LParen, "Expected '(' after for");
    const std::string firstName = consume(TokenType::Identifier, "Expected loop variable").text;

    Stmt stmt;
    stmt.line = forToken.line;
    stmt.column = forToken.column;
    if (match(TokenType::Comma)) {
        stmt.type = StmtType::ForDict;
        stmt.iterKey = firstName;
        stmt.iterValue = consume(TokenType::Identifier, "Expected value variable after ','").text;
        consume(TokenType::KeywordIn, "Expected 'in' in for-dict");
        stmt.iterable = parseExpression();
    } else {
        consume(TokenType::KeywordIn, "Expected 'in' in for");
        if (check(TokenType::Identifier) && peek().text == "range") {
            consume(TokenType::Identifier, "Expected range");
            stmt.type = StmtType::ForRange;
            stmt.iterKey = firstName;
            consume(TokenType::LParen, "Expected '(' after range");
            stmt.rangeStart = parseExpression();
            stmt.hasRangeStart = false;
            if (match(TokenType::Comma)) {
                stmt.hasRangeStart = true;
                stmt.rangeEnd = parseExpression();
            } else {
                stmt.rangeEnd = std::move(stmt.rangeStart);
                Expr zero;
                zero.type = ExprType::Number;
                zero.value = Value::Int(0);
                stmt.rangeStart = std::move(zero);
            }
            consume(TokenType::RParen, "Expected ')' after range args");
        } else {
            stmt.type = StmtType::ForList;
            stmt.iterKey = firstName;
            stmt.iterable = parseExpression();
        }
    }

    consume(TokenType::RParen, "Expected ')' after for header");
    consume(TokenType::LBrace, "Expected '{' for for-loop body");
    stmt.body = parseBlock();
    return stmt;
}

Stmt Parser::parseIfStatement() {
    const Token& ifToken = previous();
    Stmt stmt;
    stmt.type = StmtType::If;
    stmt.line = ifToken.line;
    stmt.column = ifToken.column;

    consume(TokenType::LParen, "Expected '(' after if");
    stmt.branchConditions.push_back(parseExpression());
    consume(TokenType::RParen, "Expected ')' after if condition");
    consume(TokenType::LBrace, "Expected '{' after if condition");
    stmt.branchBodies.push_back(parseBlock());

    while (match(TokenType::KeywordElif)) {
        consume(TokenType::LParen, "Expected '(' after elif");
        stmt.branchConditions.push_back(parseExpression());
        consume(TokenType::RParen, "Expected ')' after elif condition");
        consume(TokenType::LBrace, "Expected '{' after elif condition");
        stmt.branchBodies.push_back(parseBlock());
    }

    if (match(TokenType::KeywordElse)) {
        consume(TokenType::LBrace, "Expected '{' after else");
        stmt.elseBody = parseBlock();
    }

    return stmt;
}

Stmt Parser::parseWhileStatement() {
    const Token& whileToken = previous();
    Stmt stmt;
    stmt.type = StmtType::While;
    stmt.line = whileToken.line;
    stmt.column = whileToken.column;

    consume(TokenType::LParen, "Expected '(' after while");
    stmt.condition = parseExpression();
    consume(TokenType::RParen, "Expected ')' after while condition");
    consume(TokenType::LBrace, "Expected '{' after while condition");
    stmt.body = parseBlock();

    return stmt;
}

Stmt Parser::parseStatement() {
    if (match(TokenType::KeywordLet)) {
        const Token& letToken = previous();
        Stmt stmt;
        stmt.line = letToken.line;
        stmt.column = letToken.column;
        stmt.name = consume(TokenType::Identifier, "Expected variable name").text;
        consume(TokenType::Equal, "Expected '='");
        if (match(TokenType::KeywordSpawn)) {
            stmt.type = StmtType::LetSpawn;
            stmt.call = parseCallData();
        } else if (match(TokenType::KeywordAwait)) {
            stmt.type = StmtType::LetAwait;
            stmt.awaitSource = consume(TokenType::Identifier, "Expected task handle variable").text;
        } else {
            stmt.type = StmtType::LetExpr;
            stmt.expr = parseExpression();
        }
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    if (match(TokenType::KeywordFor)) {
        return parseForStatement();
    }

    if (match(TokenType::KeywordIf)) {
        return parseIfStatement();
    }

    if (match(TokenType::KeywordWhile)) {
        return parseWhileStatement();
    }

    if (match(TokenType::KeywordBreak)) {
        const Token& breakToken = previous();
        Stmt stmt;
        stmt.type = StmtType::Break;
        stmt.line = breakToken.line;
        stmt.column = breakToken.column;
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    if (match(TokenType::KeywordContinue)) {
        const Token& continueToken = previous();
        Stmt stmt;
        stmt.type = StmtType::Continue;
        stmt.line = continueToken.line;
        stmt.column = continueToken.column;
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    if (match(TokenType::KeywordReturn)) {
        const Token& returnToken = previous();
        Stmt stmt;
        stmt.type = StmtType::Return;
        stmt.line = returnToken.line;
        stmt.column = returnToken.column;
        stmt.expr = parseExpression();
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    if (match(TokenType::KeywordSleep)) {
        const Token& sleepToken = previous();
        Stmt stmt;
        stmt.type = StmtType::Sleep;
        stmt.line = sleepToken.line;
        stmt.column = sleepToken.column;
        const Value sleepValue = parseNumericLiteral(consume(TokenType::Number, "Expected millisecond number").text);
        if (!sleepValue.isInt()) {
            throw std::runtime_error(formatParseError("sleep requires integer milliseconds", previous()));
        }
        stmt.sleepMs = sleepValue;
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    if (match(TokenType::KeywordYield)) {
        const Token& yieldToken = previous();
        Stmt stmt;
        stmt.type = StmtType::Yield;
        stmt.line = yieldToken.line;
        stmt.column = yieldToken.column;
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    Stmt stmt;
    stmt.type = StmtType::Expr;
    stmt.expr = parseExpression();
    stmt.line = stmt.expr.line;
    stmt.column = stmt.expr.column;
    consume(TokenType::Semicolon, "Expected ';'");
    return stmt;
}

Expr Parser::parseExpression() {
    return parseAssignment();
}

Expr Parser::parseAssignment() {
    Expr lhs = parseEquality();
    if (!match(TokenType::Equal)) {
        return lhs;
    }

    Expr rhs = parseAssignment();
    if (lhs.type == ExprType::Variable) {
        Expr expr;
        expr.type = ExprType::AssignVariable;
        expr.line = lhs.line;
        expr.column = lhs.column;
        expr.name = lhs.name;
        expr.right = std::make_unique<Expr>(std::move(rhs));
        return expr;
    }

    if (lhs.type == ExprType::PropertyAccess) {
        Expr expr;
        expr.type = ExprType::AssignProperty;
        expr.line = lhs.line;
        expr.column = lhs.column;
        expr.object = std::move(lhs.object);
        expr.propertyName = lhs.propertyName;
        expr.right = std::make_unique<Expr>(std::move(rhs));
        return expr;
    }

    if (lhs.type == ExprType::IndexAccess) {
        Expr expr;
        expr.type = ExprType::AssignIndex;
        expr.line = lhs.line;
        expr.column = lhs.column;
        expr.object = std::move(lhs.object);
        expr.index = std::move(lhs.index);
        expr.right = std::make_unique<Expr>(std::move(rhs));
        return expr;
    }

    throw std::runtime_error(formatParseError("Only variable/property/index assignment is supported", peek()));
}

Expr Parser::parseEquality() {
    Expr expr = parseComparison();
    while (match(TokenType::EqualEqual) || match(TokenType::BangEqual)) {
        const TokenType op = previous().type;
        Expr rhs = parseComparison();

        Expr merged;
        merged.type = ExprType::Binary;
        merged.line = expr.line;
        merged.column = expr.column;
        merged.binaryOp = op;
        merged.left = std::make_unique<Expr>(std::move(expr));
        merged.right = std::make_unique<Expr>(std::move(rhs));
        expr = std::move(merged);
    }
    return expr;
}

Expr Parser::parseComparison() {
    Expr expr = parseTerm();
    while (match(TokenType::Less) || match(TokenType::LessEqual) ||
           match(TokenType::Greater) || match(TokenType::GreaterEqual)) {
        const TokenType op = previous().type;
        Expr rhs = parseTerm();

        Expr merged;
        merged.type = ExprType::Binary;
        merged.line = expr.line;
        merged.column = expr.column;
        merged.binaryOp = op;
        merged.left = std::make_unique<Expr>(std::move(expr));
        merged.right = std::make_unique<Expr>(std::move(rhs));
        expr = std::move(merged);
    }
    return expr;
}

Expr Parser::parseTerm() {
    Expr expr = parseFactor();
    while (match(TokenType::Plus) || match(TokenType::Minus)) {
        const TokenType op = previous().type;
        Expr rhs = parseFactor();

        Expr merged;
        merged.type = ExprType::Binary;
        merged.line = expr.line;
        merged.column = expr.column;
        merged.binaryOp = op;
        merged.left = std::make_unique<Expr>(std::move(expr));
        merged.right = std::make_unique<Expr>(std::move(rhs));
        expr = std::move(merged);
    }
    return expr;
}

Expr Parser::parseFactor() {
    Expr expr = parseUnary();
    while (match(TokenType::Star) || match(TokenType::Slash)) {
        const TokenType op = previous().type;
        Expr rhs = parseUnary();

        Expr merged;
        merged.type = ExprType::Binary;
        merged.line = expr.line;
        merged.column = expr.column;
        merged.binaryOp = op;
        merged.left = std::make_unique<Expr>(std::move(expr));
        merged.right = std::make_unique<Expr>(std::move(rhs));
        expr = std::move(merged);
    }
    return expr;
}

Expr Parser::parseUnary() {
    if (match(TokenType::Minus)) {
        Expr zero;
        zero.type = ExprType::Number;
        zero.value = Value::Int(0);

        Expr rhs = parseUnary();

        Expr merged;
        merged.type = ExprType::Binary;
        merged.line = rhs.line;
        merged.column = rhs.column;
        merged.binaryOp = TokenType::Minus;
        merged.left = std::make_unique<Expr>(std::move(zero));
        merged.right = std::make_unique<Expr>(std::move(rhs));
        return merged;
    }

    return parsePrimary();
}

Expr Parser::parsePrimary() {
    if (match(TokenType::Number)) {
        Expr expr;
        expr.type = ExprType::Number;
        expr.line = previous().line;
        expr.column = previous().column;
        expr.value = parseNumericLiteral(previous().text);
        return parsePostfix(std::move(expr));
    }

    if (match(TokenType::String)) {
        Expr expr;
        expr.type = ExprType::StringLiteral;
        expr.line = previous().line;
        expr.column = previous().column;
        expr.stringLiteral = previous().text;
        return parsePostfix(std::move(expr));
    }

    if (match(TokenType::LBracket)) {
        const Token& beginToken = previous();
        Expr expr;
        expr.type = ExprType::ListLiteral;
        expr.line = beginToken.line;
        expr.column = beginToken.column;
        if (!check(TokenType::RBracket)) {
            do {
                expr.listElements.push_back(parseExpression());
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RBracket, "Expected ']' in list literal");
        return parsePostfix(std::move(expr));
    }

    if (match(TokenType::LBrace)) {
        const Token& beginToken = previous();
        Expr expr;
        expr.type = ExprType::DictLiteral;
        expr.line = beginToken.line;
        expr.column = beginToken.column;
        if (!check(TokenType::RBrace)) {
            do {
                DictEntry entry;
                entry.key = std::make_unique<Expr>(parseExpression());
                consume(TokenType::Colon, "Expected ':' in dict literal");
                entry.value = std::make_unique<Expr>(parseExpression());
                expr.dictEntries.push_back(std::move(entry));
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RBrace, "Expected '}' in dict literal");
        return parsePostfix(std::move(expr));
    }

    if (match(TokenType::Identifier)) {
        Expr expr;
        expr.type = ExprType::Variable;
        expr.line = previous().line;
        expr.column = previous().column;
        expr.name = previous().text;
        return parsePostfix(std::move(expr));
    }

    if (match(TokenType::KeywordStr)) {
        Expr expr;
        expr.type = ExprType::Variable;
        expr.line = previous().line;
        expr.column = previous().column;
        expr.name = "str";
        return parsePostfix(std::move(expr));
    }

    if (match(TokenType::LParen)) {
        Expr expr = parseExpression();
        consume(TokenType::RParen, "Expected ')' in expression");
        return parsePostfix(std::move(expr));
    }

    throw std::runtime_error(formatParseError("Expected expression", peek()));
}

} // namespace gs
