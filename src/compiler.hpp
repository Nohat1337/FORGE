#pragma once

#include "ast.hpp"
#include "chunk.hpp"
#include "value.hpp"
#include "optimizer.hpp"
#include <vector>
#include <string>
#include <memory>
#include <unordered_set>

class Compiler {
public:
    Compiler();
    std::shared_ptr<ObjFunction> compile(const std::vector<StmtPtr>& program);
    void resetForRepl();
    bool hasError() const { return hadError_; }
    const std::string& errorMessage() const { return errorMessage_; }

private:
    struct Local {
        std::string name;
        int depth;
        bool isConst;
        bool captured = false;
    };
    struct Upvalue {
        uint8_t index;
        bool isLocal;
    };
    struct CompilerState {
        std::shared_ptr<ObjFunction> function;
        std::vector<Local> locals;
        std::vector<Upvalue> upvalues;
        int scopeDepth = 0;
    };
    struct ClassCompilerState {
        ClassCompilerState* enclosing = nullptr;
        bool hasSuperclass = false;
    };

    std::vector<CompilerState*> compilerStack_;
    CompilerState* current_ = nullptr;
    ClassCompilerState* currentClass_ = nullptr;
    std::unordered_set<std::string> constGlobals_;
    bool hadError_ = false;
    std::string errorMessage_;
    int currentLine_ = 1;
    bool replMode_ = false;

    void pushCompiler(std::shared_ptr<ObjFunction> fn);
    CompilerState* popCompiler();
    Chunk& currentChunk();

    void beginScope();
    void endScope();
    int addLocal(const std::string& name, bool isConst);
    int resolveLocal(const std::string& name);
    int addUpvalue(CompilerState* compiler, uint8_t index, bool isLocal);
    int resolveUpvalue(const std::string& name);

    void emitByte(uint8_t byte);
    void emitOp(uint8_t op);
    void emitOpOperand16(uint8_t op, uint16_t operand);
    int emitJump(uint8_t op);
    void patchJump(int offset);
    void emitLoop(int loopStart);
    uint16_t addConstant(const Value& value);
    void emitConstant(const Value& value);

    void compileStatements(const std::vector<StmtPtr>& stmts);
    void compileStatement(StmtPtr stmt);
    void compileVarDecl(VarDecl& decl);
    void compileFnDecl(FnDecl& decl);
    void compileGenDecl(GenDecl& decl);
    void compileClassDecl(ClassDecl& decl);
    void compileReturn(ReturnStmt& stmt);
    void compileIf(IfStmt& stmt);
    void compileWhile(WhileStmt& stmt);
    void compileFor(ForStmt& stmt);
    void compileForIn(ForInStmt& stmt);
    void compileBlock(BlockStmt& stmt);
    void compileTryCatch(TryStmt& stmt);
    void compileExprStmt(ExprStmt& stmt);
    void compileExternFn(ExternFnDecl& decl);
    void compileImport(ImportStmt& decl);

    void compileExpression(ExprPtr expr);
    void compileIntegerLiteral(IntegerLiteral& e);
    void compileFloatLiteral(FloatLiteral& e);
    void compileStringLiteral(StringLiteral& e);
    void compileBoolLiteral(BoolLiteral& e);
    void compileNilLiteral();
    void compileThisLiteral();
    void compileIdentifier(Identifier& e);
    void compileBinary(BinaryExpr& e);
    void compileUnary(UnaryExpr& e);
    void compileTernary(TernaryExpr& e);
    void compileCall(CallExpr& e);
    void compileIndex(IndexExpr& e);
    void compileMember(MemberExpr& e);
    void compileArrayLiteral(ArrayLiteral& e);
    void compileMapLiteral(MapLiteral& e);
    void compileFnExpr(FnExpr& e);
    void compileSuperExpr(SuperExpr& e);
    void compileStringInterp(StringInterpExpr& e);
    void compileMatchExpr(MatchExpr& e);
    void compileThrowExpr(ThrowExpr& e);
    void compileYieldExpr(YieldExpr& e);
    void compileAssign(AssignExpr& e);
    void compileAssignMember(AssignMemberExpr& e);
    void compileAssignIndex(AssignIndexExpr& e);

    bool isLiteralConstant(ExprPtr expr);
    bool tryFoldBinary(BinaryExpr& e);

    void error(const std::string& msg);
};
