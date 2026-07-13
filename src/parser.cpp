#include "parser.hpp"
#include <stdexcept>

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens), pos_(0) {}

Token Parser::peek() const { return tokens_[pos_]; }
Token Parser::peekNext() const {
    return (pos_ + 1 < (int)tokens_.size()) ? tokens_[pos_ + 1] : Token{TokenType::EOF_TOKEN, "", 0, 0};
}
Token Parser::advance() { lastLine_ = tokens_[pos_].line; return tokens_[pos_++]; }
StmtPtr Parser::makeStmt(Stmt::Variant v) { auto s = std::make_shared<Stmt>(std::move(v)); s->line = lastLine_; return s; }
ExprPtr Parser::makeExpr(Expr::Variant v) { auto e = std::make_shared<Expr>(std::move(v)); e->line = lastLine_; return e; }
bool Parser::check(TokenType type) const { return peek().type == type; }
bool Parser::match(TokenType type) { if (check(type)) { advance(); return true; } return false; }

Token Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    throw std::runtime_error(message + " at line " + std::to_string(peek().line) +
        ":" + std::to_string(peek().column) + " (got '" + peek().value + "')");
}

void Parser::skipNewlines() { while (check(TokenType::NEWLINE) || check(TokenType::SEMICOLON)) advance(); }

std::vector<StmtPtr> Parser::parse() {
    std::vector<StmtPtr> stmts;
    skipNewlines();
    while (!check(TokenType::EOF_TOKEN)) {
        stmts.push_back(parseStatement());
        skipNewlines();
    }
    return stmts;
}

StmtPtr Parser::parseStatement() {
    if (check(TokenType::LET)) { advance(); return parseVarDecl(false); }
    if (check(TokenType::CONST)) { advance(); return parseVarDecl(true); }
    if (check(TokenType::FN)) { advance(); return parseFnDecl(); }
    if (check(TokenType::GEN)) { advance(); return parseGenDecl(); }
    if (check(TokenType::CLASS)) return parseClassDecl();
    if (check(TokenType::RETURN)) return parseReturn();
    if (check(TokenType::IF)) return parseIf();
    if (check(TokenType::WHILE)) return parseWhile();
    if (check(TokenType::FOR)) return parseFor();
    if (check(TokenType::LBRACE)) return parseBlock();
    if (check(TokenType::TRY)) return parseTryCatch();
    if (check(TokenType::EXTERN)) return parseExternFn();
    if (check(TokenType::IMPORT)) return parseImport();
    return parseExprStmt();
}

StmtPtr Parser::parseVarDecl(bool isConst) {
    Token name = expect(TokenType::IDENTIFIER, "Expected variable name");
    expect(TokenType::ASSIGN, "Expected '=' after variable name");
    ExprPtr init = parseExpression();
    skipNewlines();
    return makeStmt(VarDecl{name.value, init, isConst});
}

StmtPtr Parser::parseFnDecl() {
    Token name = expect(TokenType::IDENTIFIER, "Expected function name");
    expect(TokenType::LPAREN, "Expected '(' after function name");
    std::vector<std::string> params;
    if (!check(TokenType::RPAREN)) {
        do { Token p = expect(TokenType::IDENTIFIER, "Expected parameter name"); params.push_back(p.value); }
        while (match(TokenType::COMMA));
    }
    expect(TokenType::RPAREN, "Expected ')'");
    auto body = std::get<BlockStmt>(parseBlock()->node).statements;
    return makeStmt(FnDecl{name.value, params, body});
}

StmtPtr Parser::parseGenDecl() {
    Token name = expect(TokenType::IDENTIFIER, "Expected generator function name");
    expect(TokenType::LPAREN, "Expected '(' after generator name");
    std::vector<std::string> params;
    if (!check(TokenType::RPAREN)) {
        do { Token p = expect(TokenType::IDENTIFIER, "Expected parameter name"); params.push_back(p.value); }
        while (match(TokenType::COMMA));
    }
    expect(TokenType::RPAREN, "Expected ')'");
    expect(TokenType::LBRACE, "Expected '{'");
    skipNewlines();
    std::vector<StmtPtr> body;
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        body.push_back(parseStatement());
        skipNewlines();
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return makeStmt(GenDecl{name.value, params, body});
}

StmtPtr Parser::parseImport() {
    advance();
    Token path = expect(TokenType::STRING_PART, "Expected module name");
    std::string moduleName = path.value;
    std::string alias;
    if (match(TokenType::AS)) {
        Token a = expect(TokenType::IDENTIFIER, "Expected alias name");
        alias = a.value;
    }
    std::vector<std::string> selectiveImports;
    ImportStmt stmt;
    stmt.moduleName = moduleName;
    stmt.alias = alias;
    stmt.selectiveImports = selectiveImports;
    return makeStmt(stmt);
}

StmtPtr Parser::parseReturn() {
    advance();
    ExprPtr value = nullptr;
    if (!check(TokenType::NEWLINE) && !check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN))
        value = parseExpression();
    skipNewlines();
    return makeStmt(ReturnStmt{value});
}

StmtPtr Parser::parseIf() {
    advance();
    expect(TokenType::LPAREN, "Expected '(' after 'if'");
    ExprPtr cond = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    auto thenBlock = std::get<BlockStmt>(parseBlock()->node).statements;
    std::vector<StmtPtr> elseBranch;
    if (match(TokenType::ELSE)) {
        if (check(TokenType::IF)) { advance(); elseBranch.push_back(parseIf()); }
        else { elseBranch = std::get<BlockStmt>(parseBlock()->node).statements; }
    }
    return makeStmt(IfStmt{cond, thenBlock, elseBranch});
}

StmtPtr Parser::parseWhile() {
    advance();
    expect(TokenType::LPAREN, "Expected '(' after 'while'");
    ExprPtr cond = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    auto body = std::get<BlockStmt>(parseBlock()->node).statements;
    return makeStmt(WhileStmt{cond, body});
}

StmtPtr Parser::parseFor() {
    advance();
    expect(TokenType::LPAREN, "Expected '('");
    skipNewlines();

    if (check(TokenType::LET)) {
        advance();
        Token name = expect(TokenType::IDENTIFIER, "Expected variable name");
        if (check(TokenType::IN)) {
            advance();
            ExprPtr iterable = parseExpression();
            expect(TokenType::RPAREN, "Expected ')'");
            auto body = std::get<BlockStmt>(parseBlock()->node).statements;
            return makeStmt(ForInStmt{name.value, iterable, body});
        }
        expect(TokenType::ASSIGN, "Expected '=' or 'in'");
        ExprPtr val = parseExpression();
        auto init = makeStmt(VarDecl{name.value, val, false});
        expect(TokenType::SEMICOLON, "Expected ';'");
        skipNewlines();
        ExprPtr cond = nullptr;
        if (!check(TokenType::SEMICOLON)) cond = parseExpression();
        expect(TokenType::SEMICOLON, "Expected ';'");
        skipNewlines();
        ExprPtr incr = nullptr;
        if (!check(TokenType::RPAREN)) incr = parseExpression();
        expect(TokenType::RPAREN, "Expected ')'");
        auto body = std::get<BlockStmt>(parseBlock()->node).statements;
        return makeStmt(ForStmt{init, cond, incr, body});
    }

    StmtPtr init = nullptr;
    if (check(TokenType::CONST)) {
        advance();
        Token name = expect(TokenType::IDENTIFIER, "Expected variable name");
        expect(TokenType::ASSIGN, "Expected '='");
        ExprPtr val = parseExpression();
        init = makeStmt(VarDecl{name.value, val, true});
    } else if (!check(TokenType::SEMICOLON)) {
        init = makeStmt(ExprStmt{parseExpression()});
    }
    expect(TokenType::SEMICOLON, "Expected ';'");
    skipNewlines();
    ExprPtr cond = nullptr;
    if (!check(TokenType::SEMICOLON)) cond = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';'");
    skipNewlines();
    ExprPtr incr = nullptr;
    if (!check(TokenType::RPAREN)) incr = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    auto body = std::get<BlockStmt>(parseBlock()->node).statements;
    return makeStmt(ForStmt{init, cond, incr, body});
}

StmtPtr Parser::parseBlock() {
    expect(TokenType::LBRACE, "Expected '{'");
    skipNewlines();
    std::vector<StmtPtr> stmts;
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        stmts.push_back(parseStatement());
        skipNewlines();
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return makeStmt(BlockStmt{stmts});
}

StmtPtr Parser::parseExprStmt() {
    ExprPtr expr = parseExpression();
    skipNewlines();
    return makeStmt(ExprStmt{expr});
}

StmtPtr Parser::parseClassDecl() {
    advance();
    Token name = expect(TokenType::IDENTIFIER, "Expected class name");
    ExprPtr superclass = nullptr;
    if (match(TokenType::EXTENDS)) {
        superclass = makeExpr(Identifier{expect(TokenType::IDENTIFIER, "Expected superclass name").value});
    }
    expect(TokenType::LBRACE, "Expected '{'");
    skipNewlines();
    std::vector<FnDecl> methods;
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        expect(TokenType::FN, "Expected 'fn' for method");
        Token methodName = expect(TokenType::IDENTIFIER, "Expected method name");
        expect(TokenType::LPAREN, "Expected '('");
        std::vector<std::string> params;
        if (!check(TokenType::RPAREN)) {
            do { Token p = expect(TokenType::IDENTIFIER, "Expected parameter"); params.push_back(p.value); }
            while (match(TokenType::COMMA));
        }
        expect(TokenType::RPAREN, "Expected ')'");
        auto body = std::get<BlockStmt>(parseBlock()->node).statements;
        methods.push_back(FnDecl{methodName.value, params, body});
        skipNewlines();
    }
    expect(TokenType::RBRACE, "Expected '}'");
    skipNewlines();
    return makeStmt(ClassDecl{name.value, methods, superclass});
}

StmtPtr Parser::parseTryCatch() {
    advance();
    auto body = std::get<BlockStmt>(parseBlock()->node).statements;
    expect(TokenType::CATCH, "Expected 'catch'");
    expect(TokenType::LPAREN, "Expected '(' after 'catch'");
    Token var = expect(TokenType::IDENTIFIER, "Expected error variable name");
    expect(TokenType::RPAREN, "Expected ')'");
    auto catchBody = std::get<BlockStmt>(parseBlock()->node).statements;
    skipNewlines();
    return makeStmt(TryStmt{body, var.value, catchBody});
}

StmtPtr Parser::parseExternFn() {
    advance();
    Token name = expect(TokenType::IDENTIFIER, "Expected function name");
    expect(TokenType::LPAREN, "Expected '('");
    std::vector<std::string> params;
    if (!check(TokenType::RPAREN)) {
        do { Token p = expect(TokenType::IDENTIFIER, "Expected parameter"); params.push_back(p.value); }
        while (match(TokenType::COMMA));
    }
    expect(TokenType::RPAREN, "Expected ')'");
    expect(TokenType::STRING_PART, "Expected library path");
    Token lib = tokens_[pos_ - 1];
    skipNewlines();
    return makeStmt(ExternFnDecl{name.value, params, lib.value});
}

// ---- Expression Parsing ----

ExprPtr Parser::parseExpression() { return parseAssignment(); }

ExprPtr Parser::parseAssignment() {
    ExprPtr expr = parseTernary();
    if (check(TokenType::ASSIGN) || check(TokenType::PLUS_ASSIGN) || check(TokenType::MINUS_ASSIGN)) {
        Token op = advance();
        ExprPtr rhs = parseAssignment();
        if (auto* id = std::get_if<Identifier>(&expr->node)) {
            if (op.type == TokenType::ASSIGN) return makeExpr(AssignExpr{id->name, rhs});
            Token binOp = (op.type == TokenType::PLUS_ASSIGN) ? Token{TokenType::PLUS, "+", op.line, op.column} : Token{TokenType::MINUS, "-", op.line, op.column};
            return makeExpr(AssignExpr{id->name, makeExpr(BinaryExpr{makeExpr(Identifier{id->name}), binOp, rhs})});
        }
        if (auto* mem = std::get_if<MemberExpr>(&expr->node))
            return makeExpr(AssignMemberExpr{mem->object, mem->member, rhs});
        if (auto* idx = std::get_if<IndexExpr>(&expr->node))
            return makeExpr(AssignIndexExpr{idx->object, idx->index, rhs});
        throw std::runtime_error("Invalid assignment target at line " + std::to_string(op.line));
    }
    return expr;
}

ExprPtr Parser::parseTernary() {
    ExprPtr expr = parseOr();
    if (match(TokenType::QUESTION)) {
        ExprPtr thenExpr = parseExpression();
        expect(TokenType::COLON, "Expected ':' in ternary expression");
        ExprPtr elseExpr = parseExpression();
        return makeExpr(TernaryExpr{expr, thenExpr, elseExpr});
    }
    return expr;
}

ExprPtr Parser::parseOr() {
    ExprPtr left = parseAnd();
    while (check(TokenType::OR)) {
        Token op = advance();
        left = makeExpr(BinaryExpr{left, op, parseAnd()});
    }
    return left;
}

ExprPtr Parser::parseAnd() {
    ExprPtr left = parseEquality();
    while (check(TokenType::AND)) {
        Token op = advance();
        left = makeExpr(BinaryExpr{left, op, parseEquality()});
    }
    return left;
}

ExprPtr Parser::parseEquality() {
    ExprPtr left = parseComparison();
    while (check(TokenType::EQUAL_EQUAL) || check(TokenType::BANG_EQUAL)) {
        Token op = advance();
        left = makeExpr(BinaryExpr{left, op, parseComparison()});
    }
    return left;
}

ExprPtr Parser::parseComparison() {
    ExprPtr left = parseAddition();
    while (check(TokenType::LESS) || check(TokenType::LESS_EQUAL) || check(TokenType::GREATER) || check(TokenType::GREATER_EQUAL)) {
        Token op = advance();
        left = makeExpr(BinaryExpr{left, op, parseAddition()});
    }
    return left;
}

ExprPtr Parser::parseAddition() {
    ExprPtr left = parseMultiplication();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        Token op = advance();
        left = makeExpr(BinaryExpr{left, op, parseMultiplication()});
    }
    return left;
}

ExprPtr Parser::parseMultiplication() {
    ExprPtr left = parseUnary();
    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT)) {
        Token op = advance();
        left = makeExpr(BinaryExpr{left, op, parseUnary()});
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (check(TokenType::MINUS) || check(TokenType::NOT)) {
        Token op = advance();
        return makeExpr(UnaryExpr{op, parseUnary()});
    }
    if (check(TokenType::THROW)) {
        advance();
        return makeExpr(ThrowExpr{parseExpression()});
    }
    return parseCall();
}

ExprPtr Parser::parseCall() {
    ExprPtr expr = parsePrimary();
    while (true) {
        if (match(TokenType::LPAREN)) {
            std::vector<ExprPtr> args;
            if (!check(TokenType::RPAREN)) {
                do { args.push_back(parseExpression()); } while (match(TokenType::COMMA));
            }
            expect(TokenType::RPAREN, "Expected ')'");
            expr = makeExpr(CallExpr{expr, args});
        } else if (match(TokenType::DOT)) {
            Token member = expect(TokenType::IDENTIFIER, "Expected property name");
            expr = makeExpr(MemberExpr{expr, member.value});
        } else if (match(TokenType::LBRACKET)) {
            ExprPtr index = parseExpression();
            expect(TokenType::RBRACKET, "Expected ']'");
            expr = makeExpr(IndexExpr{expr, index});
        } else break;
    }
    return expr;
}

ExprPtr Parser::parsePrimary() {
    if (check(TokenType::INTEGER)) { Token t = advance(); return makeExpr(IntegerLiteral{std::stoll(t.value)}); }
    if (check(TokenType::FLOAT)) { Token t = advance(); return makeExpr(FloatLiteral{std::stod(t.value)}); }
    if (check(TokenType::STRING_PART) || check(TokenType::DOLLAR_LBRACE) || check(TokenType::STRING_END)) return parseStringInterp();
    if (check(TokenType::TRUE)) { advance(); return makeExpr(BoolLiteral{true}); }
    if (check(TokenType::FALSE)) { advance(); return makeExpr(BoolLiteral{false}); }
    if (check(TokenType::NIL)) { advance(); return makeExpr(NilLiteral{}); }
    if (check(TokenType::THIS)) { advance(); return makeExpr(ThisLiteral{}); }
    if (check(TokenType::IDENTIFIER)) { Token t = advance(); return makeExpr(Identifier{t.value}); }
    if (check(TokenType::MATCH)) return parseMatchExpr();
    if (check(TokenType::FN)) { advance(); return parseFnExpr(); }
    if (check(TokenType::YIELD)) {
        advance();
        ExprPtr val = nullptr;
        if (!check(TokenType::RBRACE) && !check(TokenType::NEWLINE) && !check(TokenType::SEMICOLON) && !check(TokenType::EOF_TOKEN)) {
            val = parseExpression();
        }
        return makeExpr(YieldExpr{val});
    }
    if (check(TokenType::LBRACKET)) return parseArray();
    if (check(TokenType::LBRACE)) return parseMap();
    if (check(TokenType::LPAREN)) {
        advance();
        ExprPtr expr = parseExpression();
        expect(TokenType::RPAREN, "Expected ')'");
        return expr;
    }
    throw std::runtime_error("Unexpected token '" + peek().value + "' at line " + std::to_string(peek().line));
}

ExprPtr Parser::parseArray() {
    expect(TokenType::LBRACKET, "Expected '['");
    std::vector<ExprPtr> elements;
    if (!check(TokenType::RBRACKET)) {
        do { elements.push_back(parseExpression()); } while (match(TokenType::COMMA));
    }
    expect(TokenType::RBRACKET, "Expected ']'");
    return makeExpr(ArrayLiteral{elements});
}

ExprPtr Parser::parseMap() {
    expect(TokenType::LBRACE, "Expected '{'");
    std::vector<MapEntry> entries;
    if (!check(TokenType::RBRACE)) {
        do {
            ExprPtr key = parseExpression();
            expect(TokenType::COLON, "Expected ':' after map key");
            ExprPtr value = parseExpression();
            entries.push_back({key, value});
        } while (match(TokenType::COMMA));
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return makeExpr(MapLiteral{entries});
}

ExprPtr Parser::parseFnExpr() {
    expect(TokenType::LPAREN, "Expected '(' after 'fn'");
    std::vector<std::string> params;
    if (!check(TokenType::RPAREN)) {
        do { Token p = expect(TokenType::IDENTIFIER, "Expected parameter"); params.push_back(p.value); }
        while (match(TokenType::COMMA));
    }
    expect(TokenType::RPAREN, "Expected ')'");
    auto body = std::get<BlockStmt>(parseBlock()->node).statements;
    return makeExpr(FnExpr{params, body});
}

ExprPtr Parser::parseMatchExpr() {
    advance();
    ExprPtr value = parseExpression();
    expect(TokenType::LBRACE, "Expected '{' after match");
    skipNewlines();
    std::vector<MatchCase> cases;
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        expect(TokenType::CASE, "Expected 'case'");
        MatchPattern pattern;
        if (check(TokenType::WILDCARD)) { advance(); pattern.type = MatchPattern::WILDCARD; }
        else if (check(TokenType::IDENTIFIER)) {
            Token t = advance();
            if (check(TokenType::COLON)) {
                advance();
                pattern.type = MatchPattern::IDENTIFIER; pattern.name = t.value;
            } else {
                pattern.type = MatchPattern::LITERAL;
                pattern.literal = makeExpr(Identifier{t.value});
                pos_--; // back up
            }
        } else {
            pattern.type = MatchPattern::LITERAL;
            pattern.literal = parseExpression();
        }
        expect(TokenType::COLON, "Expected ':'");
        std::vector<StmtPtr> body;
        body.push_back(parseStatement());
        if (body.size() == 1) {
            if (auto* bs = std::get_if<BlockStmt>(&body[0]->node)) body = bs->statements;
        }
        cases.push_back(MatchCase{pattern, body});
        skipNewlines();
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return makeExpr(MatchExpr{value, cases});
}

ExprPtr Parser::parseStringInterp() {
    std::vector<StringInterpPart> parts;
    while (!check(TokenType::STRING_END)) {
        if (check(TokenType::STRING_PART)) {
            Token t = advance();
            parts.push_back({false, t.value, nullptr});
        } else if (check(TokenType::DOLLAR_LBRACE)) {
            advance();
            ExprPtr expr = parseExpression();
            expect(TokenType::RBRACE, "Expected '}'");
            parts.push_back({true, "", expr});
        } else {
            throw std::runtime_error("Unexpected token in string interpolation at line " + std::to_string(peek().line));
        }
    }
    expect(TokenType::STRING_END, "Expected closing '\"'");
    if (parts.size() == 1 && !parts[0].isExpr) {
        return makeExpr(StringLiteral{parts[0].text});
    }
    return makeExpr(StringInterpExpr{parts});
}
