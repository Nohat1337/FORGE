#pragma once

#include <vector>
#include <stdexcept>
#include "lexer.hpp"
#include "ast.hpp"

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    std::vector<StmtPtr> parse();

private:
    std::vector<Token> tokens_;
    int pos_;
    int lastLine_ = 1;
    int depth_ = 0;

    // RAII guard against stack exhaustion from deeply nested source
    // (e.g. "((((((...))))))" bombs). Throws a clean error past the limit.
    static constexpr int MAX_DEPTH = 512;
    struct DepthGuard {
        Parser& p;
        explicit DepthGuard(Parser& parser) : p(parser) {
            if (++p.depth_ > MAX_DEPTH)
                throw std::runtime_error("Maximum nesting depth exceeded at line " +
                                         std::to_string(p.peek().line));
        }
        ~DepthGuard() { --p.depth_; }
    };

    Token peek() const;
    Token peekNext() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token expect(TokenType type, const std::string& message);
    void skipNewlines();

    StmtPtr makeStmt(Stmt::Variant v);
    ExprPtr makeExpr(Expr::Variant v);

    StmtPtr parseStatement();
    StmtPtr parseVarDecl(bool isConst);
    StmtPtr parseFnDecl();
    StmtPtr parseGenDecl();
    StmtPtr parseImport();
    StmtPtr parseReturn();
    StmtPtr parseIf();
    StmtPtr parseWhile();
    StmtPtr parseFor();
    StmtPtr parseBlock();
    StmtPtr parseExprStmt();
    StmtPtr parseClassDecl();
    StmtPtr parseStructDecl();
    StmtPtr parseImplDecl();
    StmtPtr parseEnumDecl();
    StmtPtr parseTryCatch();
    StmtPtr parseExternFn();

    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseTernary();
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseBitwiseOr();
    ExprPtr parseBitwiseXor();
    ExprPtr parseBitwiseAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseAddition();
    ExprPtr parseMultiplication();
    ExprPtr parseUnary();
    ExprPtr parseCall();
    ExprPtr parsePrimary();
    ExprPtr parseArray();
    ExprPtr parseMap();
    ExprPtr parseFnExpr();
    ExprPtr parseMatchExpr();
    ExprPtr parseStringInterp();
};
