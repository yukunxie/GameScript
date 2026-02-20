#include "gs/parser.hpp"

#include <stdexcept>

namespace gs {

namespace {

std::int64_t parseNumericLiteral(const std::string& text) {
    return static_cast<std::int64_t>(std::stod(text));
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
        } else if (check(TokenType::KeywordLet)) {
            Stmt stmt = parseStatement();
            if (stmt.type != StmtType::LetExpr) {
                throw std::runtime_error("Top-level let only supports direct expression assignment");
            }
            program.topLevelLets.push_back(std::move(stmt));
        } else {
            throw std::runtime_error("Top-level statement must be class, fn, or let");
        }
    }
    return program;
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
    throw std::runtime_error(message);
}

ClassDecl Parser::parseClass() {
    consume(TokenType::KeywordClass, "Expected 'class'");
    ClassDecl cls;
    cls.name = consume(TokenType::Identifier, "Expected class name").text;
    if (match(TokenType::KeywordExtends)) {
        cls.baseName = consume(TokenType::Identifier, "Expected base class name").text;
    }
    consume(TokenType::LBrace, "Expected '{' after class name");
    while (!check(TokenType::RBrace) && !isAtEnd()) {
        if (check(TokenType::KeywordFn)) {
            cls.methods.push_back(parseFunction());
        } else {
            ClassAttrDecl attr;
            attr.name = consume(TokenType::Identifier, "Expected attribute name").text;
            consume(TokenType::Equal, "Expected '=' after attribute name");
            attr.initializer = parseExpression();
            consume(TokenType::Semicolon, "Expected ';' after attribute declaration");
            cls.attributes.push_back(std::move(attr));
        }
    }
    consume(TokenType::RBrace, "Expected '}' after class body");
    return cls;
}

FunctionDecl Parser::parseFunction() {
    consume(TokenType::KeywordFn, "Expected 'fn'");
    FunctionDecl fn;
    fn.name = consume(TokenType::Identifier, "Expected function name").text;
    consume(TokenType::LParen, "Expected '('");

    if (!check(TokenType::RParen)) {
        do {
            fn.params.push_back(consume(TokenType::Identifier, "Expected parameter name").text);
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RParen, "Expected ')'");
    consume(TokenType::LBrace, "Expected '{'");
    fn.body = parseBlock();
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
            prop.object = std::make_unique<Expr>(std::move(expr));
            prop.propertyName = memberName;
            expr = std::move(prop);
            continue;
        }

        if (match(TokenType::LBracket)) {
            Expr indexAccess;
            indexAccess.type = ExprType::IndexAccess;
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
    consume(TokenType::LParen, "Expected '(' after for");
    const std::string firstName = consume(TokenType::Identifier, "Expected loop variable").text;

    Stmt stmt;
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
    Stmt stmt;
    stmt.type = StmtType::If;

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
    Stmt stmt;
    stmt.type = StmtType::While;

    consume(TokenType::LParen, "Expected '(' after while");
    stmt.condition = parseExpression();
    consume(TokenType::RParen, "Expected ')' after while condition");
    consume(TokenType::LBrace, "Expected '{' after while condition");
    stmt.body = parseBlock();

    return stmt;
}

Stmt Parser::parseStatement() {
    if (match(TokenType::KeywordLet)) {
        Stmt stmt;
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
        Stmt stmt;
        stmt.type = StmtType::Break;
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    if (match(TokenType::KeywordContinue)) {
        Stmt stmt;
        stmt.type = StmtType::Continue;
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    if (match(TokenType::KeywordReturn)) {
        Stmt stmt;
        stmt.type = StmtType::Return;
        stmt.expr = parseExpression();
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    if (match(TokenType::KeywordSleep)) {
        Stmt stmt;
        stmt.type = StmtType::Sleep;
        stmt.sleepMs = Value::Int(parseNumericLiteral(consume(TokenType::Number, "Expected millisecond number").text));
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    if (match(TokenType::KeywordYield)) {
        Stmt stmt;
        stmt.type = StmtType::Yield;
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    Stmt stmt;
    stmt.type = StmtType::Expr;
    stmt.expr = parseExpression();
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
    if (lhs.type == ExprType::PropertyAccess) {
        Expr expr;
        expr.type = ExprType::AssignProperty;
        expr.object = std::move(lhs.object);
        expr.propertyName = lhs.propertyName;
        expr.right = std::make_unique<Expr>(std::move(rhs));
        return expr;
    }

    if (lhs.type == ExprType::IndexAccess) {
        Expr expr;
        expr.type = ExprType::AssignIndex;
        expr.object = std::move(lhs.object);
        expr.index = std::move(lhs.index);
        expr.right = std::make_unique<Expr>(std::move(rhs));
        return expr;
    }

    throw std::runtime_error("Only object property or index assignment is supported");
}

Expr Parser::parseEquality() {
    Expr expr = parseComparison();
    while (match(TokenType::EqualEqual) || match(TokenType::BangEqual)) {
        const TokenType op = previous().type;
        Expr rhs = parseComparison();

        Expr merged;
        merged.type = ExprType::Binary;
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
        expr.value = Value::Int(parseNumericLiteral(previous().text));
        return parsePostfix(std::move(expr));
    }

    if (match(TokenType::String)) {
        Expr expr;
        expr.type = ExprType::StringLiteral;
        expr.stringLiteral = previous().text;
        return parsePostfix(std::move(expr));
    }

    if (match(TokenType::LBracket)) {
        Expr expr;
        expr.type = ExprType::ListLiteral;
        if (!check(TokenType::RBracket)) {
            do {
                expr.listElements.push_back(parseExpression());
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RBracket, "Expected ']' in list literal");
        return parsePostfix(std::move(expr));
    }

    if (match(TokenType::LBrace)) {
        Expr expr;
        expr.type = ExprType::DictLiteral;
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
        expr.name = previous().text;
        return parsePostfix(std::move(expr));
    }

    if (match(TokenType::KeywordStr)) {
        Expr expr;
        expr.type = ExprType::Variable;
        expr.name = "str";
        return parsePostfix(std::move(expr));
    }

    if (match(TokenType::LParen)) {
        Expr expr = parseExpression();
        consume(TokenType::RParen, "Expected ')' in expression");
        return parsePostfix(std::move(expr));
    }

    throw std::runtime_error("Expected expression");
}

} // namespace gs
