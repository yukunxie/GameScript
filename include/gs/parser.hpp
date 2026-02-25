#pragma once

#include "gs/bytecode.hpp"
#include "gs/tokenizer.hpp"

#include <memory>
#include <string>
#include <vector>

namespace gs {

struct Expr;
struct FunctionDecl;

enum class ExprType {
    Number,
    StringLiteral,
    Variable,
    Unary,
    Binary,
    ListLiteral,
    DictLiteral,
    Call,
    MethodCall,
    PropertyAccess,
    IndexAccess,
    Lambda,
    AssignVariable,
    AssignProperty,
    AssignIndex
};

struct DictEntry {
    std::unique_ptr<Expr> key;
    std::unique_ptr<Expr> value;
};

struct Expr {
    ExprType type{ExprType::Number};
    std::size_t line{0};
    std::size_t column{0};
    Value value{Value::Nil()};
    std::string name;
    std::string stringLiteral;
    TokenType unaryOp{TokenType::Bang};
    TokenType binaryOp{TokenType::Plus};
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    std::vector<Expr> listElements;
    std::vector<DictEntry> dictEntries;
    std::unique_ptr<Expr> callee;
    std::vector<Expr> args;
    std::unique_ptr<Expr> object;
    std::string methodName;
    std::string propertyName;
    std::unique_ptr<Expr> index;
    std::unique_ptr<FunctionDecl> lambdaDecl;
};

enum class StmtType {
    LetExpr,
    LetSpawn,
    LetAwait,
    ForRange,
    ForList,
    ForDict,
    If,
    While,
    Break,
    Continue,
    Expr,
    Return,
    Sleep,
    Yield
};

struct CallData {
    std::string callee;
    std::vector<Expr> args;
};

struct Stmt {
    StmtType type{StmtType::Yield};
    std::size_t line{0};
    std::size_t column{0};
    std::string name;
    Expr expr;
    CallData call;
    std::string awaitSource;
    Value sleepMs{Value::Int(0)};
    bool hasRangeStart{false};
    std::string iterKey;
    std::string iterValue;
    Expr rangeStart;
    Expr rangeEnd;
    Expr iterable;
    Expr condition;
    std::vector<Expr> branchConditions;
    std::vector<std::vector<Stmt>> branchBodies;
    std::vector<Stmt> elseBody;
    std::vector<Stmt> body;
};

struct FunctionDecl {
    std::size_t line{0};
    std::size_t column{0};
    std::string name;
    std::vector<std::string> params;
    std::vector<Stmt> body;
};

struct ClassAttrDecl {
    std::size_t line{0};
    std::size_t column{0};
    std::string name;
    Expr initializer;
};

struct ClassDecl {
    std::size_t line{0};
    std::size_t column{0};
    std::string name;
    std::string baseName;
    std::vector<ClassAttrDecl> attributes;
    std::vector<FunctionDecl> methods;
};

struct Program {
    std::vector<ClassDecl> classes;
    std::vector<FunctionDecl> functions;
    std::vector<Stmt> topLevelStatements;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parseProgram();

private:
    const Token& peek() const;
    const Token& previous() const;
    bool isAtEnd() const;
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& consume(TokenType type, const char* message);

    ClassDecl parseClass();
    FunctionDecl parseFunction();
    Stmt parseStatement();
    Stmt parseForStatement();
    Stmt parseIfStatement();
    Stmt parseWhileStatement();
    std::vector<Stmt> parseBlock();
    CallData parseCallData();
    Expr parsePostfix(Expr expr);
    Expr parseExpression();
    Expr parseAssignment();
    Expr parseLogicalOr();
    Expr parseLogicalAnd();
    Expr parseBitwiseOr();
    Expr parseBitwiseXor();
    Expr parseBitwiseAnd();
    Expr parseEquality();
    Expr parseComparison();
    Expr parseMembership();
    Expr parseShift();
    Expr parseTerm();
    Expr parseFactor();
    Expr parseUnary();
    Expr parsePower();
    Expr parsePrimary();
    bool isLambdaStart() const;
    std::string currentScopeName() const;
    std::string formatParseError(const char* message, const Token& token) const;

    std::vector<Token> tokens_;
    std::size_t current_{0};
    std::string currentClassName_;
    std::string currentFunctionName_;
};

} // namespace gs
