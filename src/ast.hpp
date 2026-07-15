#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include "lexer.hpp"

struct Expr;
struct Stmt;
using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;

struct IntegerLiteral { long long value; };
struct FloatLiteral { double value; };
struct CharLiteral { char value; };
struct StringLiteral { std::string value; };
struct BoolLiteral { bool value; };
struct NilLiteral {};
struct ThisLiteral {};
struct Identifier { std::string name; };

struct BinaryExpr {
    ExprPtr left;
    Token op;
    ExprPtr right;
};

struct UnaryExpr {
    Token op;
    ExprPtr operand;
};

struct TernaryExpr {
    ExprPtr condition;
    ExprPtr thenExpr;
    ExprPtr elseExpr;
};

struct CallExpr {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;
};

struct IndexExpr {
    ExprPtr object;
    ExprPtr index;
};

struct MemberExpr {
    ExprPtr object;
    std::string member;
};

struct ArrayLiteral {
    std::vector<ExprPtr> elements;
};

struct MapEntry {
    ExprPtr key;
    ExprPtr value;
};

struct MapLiteral {
    std::vector<MapEntry> entries;
};

struct FnExpr {
    std::vector<std::string> params;
    std::vector<StmtPtr> body;
};

struct SuperExpr {
    std::string method;
};

struct StringInterpPart {
    bool isExpr;
    std::string text;
    ExprPtr expression;
};

struct StringInterpExpr {
    std::vector<StringInterpPart> parts;
};

struct MatchPattern {
    enum Type { LITERAL, IDENTIFIER, WILDCARD, RANGE, GUARD, ARRAY_DESTRUCTURE } type;
    ExprPtr literal;
    std::string name;
    ExprPtr rangeLow;
    ExprPtr rangeHigh;
    ExprPtr guardCondition;
    std::vector<MatchPattern> elements;
    bool hasRest = false;
    std::string restName;
};

struct MatchCase {
    MatchPattern pattern;
    std::vector<StmtPtr> body;
};

struct MatchExpr {
    ExprPtr value;
    std::vector<MatchCase> cases;
};

struct ThrowExpr {
    ExprPtr value;
};

struct YieldExpr {
    ExprPtr value;
};

struct AssignExpr {
    std::string name;
    ExprPtr value;
};

struct AssignMemberExpr {
    ExprPtr object;
    std::string member;
    ExprPtr value;
};

struct AssignIndexExpr {
    ExprPtr object;
    ExprPtr index;
    ExprPtr value;
};

struct Expr {
    using Variant = std::variant<
        IntegerLiteral, FloatLiteral, CharLiteral, StringLiteral, BoolLiteral, NilLiteral,
        ThisLiteral, Identifier, BinaryExpr, UnaryExpr, TernaryExpr,
        CallExpr, IndexExpr, MemberExpr, ArrayLiteral, MapLiteral, FnExpr,
        SuperExpr, StringInterpExpr, MatchExpr, ThrowExpr, YieldExpr,
        AssignExpr, AssignMemberExpr, AssignIndexExpr
    >;
    Variant node;
    int line = 0;
    Expr() = default;
    template<typename T> Expr(T&& v) : node(std::forward<T>(v)) {}
};

struct ExprStmt { ExprPtr expression; };
struct VarDecl {
    std::string name;
    ExprPtr initializer;
    bool isConst;
};
struct ReturnStmt { ExprPtr value; };
struct IfStmt {
    ExprPtr condition;
    std::vector<StmtPtr> thenBranch;
    std::vector<StmtPtr> elseBranch;
};
struct WhileStmt {
    ExprPtr condition;
    std::vector<StmtPtr> body;
};
struct ForStmt {
    StmtPtr initializer;
    ExprPtr condition;
    ExprPtr increment;
    std::vector<StmtPtr> body;
};
struct ForInStmt {
    std::string variable;
    ExprPtr iterable;
    std::vector<StmtPtr> body;
};
struct BlockStmt { std::vector<StmtPtr> statements; };
struct FnDecl {
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtPtr> body;
};

struct GenDecl {
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtPtr> body;
};

struct ClassDecl {
    std::string name;
    std::vector<FnDecl> methods;
    ExprPtr superclass;
};

struct StructDecl {
    std::string name;
    std::vector<std::string> fields;
};

struct ImplDecl {
    std::string className;
    std::vector<FnDecl> methods;
};

struct EnumDecl {
    std::string name;
    std::vector<std::string> variants;
};

struct TryStmt {
    std::vector<StmtPtr> body;
    std::string catchVar;
    std::vector<StmtPtr> catchBody;
};

struct ExternFnDecl {
    std::string name;
    std::vector<std::string> params;
    std::string library;
};

struct BreakStmt {};
struct ContinueStmt {};

struct ImportStmt {
    std::string moduleName;
    std::string alias;
    std::vector<std::string> selectiveImports;
};

struct Stmt {
    using Variant = std::variant<
        ExprStmt, VarDecl, ReturnStmt, IfStmt, WhileStmt,
        ForStmt, ForInStmt, BlockStmt, FnDecl, GenDecl, ClassDecl,
        StructDecl, ImplDecl, EnumDecl,
        TryStmt, ExternFnDecl, ImportStmt, BreakStmt, ContinueStmt
    >;
    Variant node;
    int line = 0;
    Stmt() = default;
    template<typename T> Stmt(T&& v) : node(std::forward<T>(v)) {}
};
