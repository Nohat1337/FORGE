#pragma once

#include <vector>
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
