#include "compiler.hpp"
#include "optimizer.hpp"
#include <stdexcept>
#include <iostream>
#include <cmath>

Compiler::Compiler() {}

void Compiler::resetForRepl() {
    for (auto* s : compilerStack_) delete s;
    compilerStack_.clear();
    current_ = nullptr;
    currentClass_ = nullptr;
    hadError_ = false;
    errorMessage_.clear();
    currentLine_ = 1;
    replMode_ = true;
}

void Compiler::error(const std::string& msg) {
    hadError_ = true;
    errorMessage_ = msg;
    throw std::runtime_error(msg);
}

Chunk& Compiler::currentChunk() { return *current_->function->chunk; }

void Compiler::pushCompiler(std::shared_ptr<ObjFunction> fn) {
    auto* state = new Compiler::CompilerState();
    state->function = fn;
    state->scopeDepth = 0;
    state->locals.push_back({"", 0, false}); // slot 0 = hidden receiver
    compilerStack_.push_back(state);
    current_ = state;
}

Compiler::CompilerState* Compiler::popCompiler() {
    auto* prev = current_;
    if (compilerStack_.size() > 1) {
        compilerStack_.pop_back();
        current_ = compilerStack_.back();
    }
    return prev;
}

void Compiler::beginScope() { current_->scopeDepth++; }

void Compiler::endScope() {
    int depth = current_->scopeDepth;
    int count = 0;
    for (int i = (int)current_->locals.size() - 1; i >= 0; i--) {
        if (current_->locals[i].depth >= depth) { count++; }
        else break;
    }
    for (int i = 0; i < count; i++) {
        int slot = (int)current_->locals.size() - 1 - i;
        if (current_->locals[slot].captured) {
            emitOpOperand16(OP_CLOSE_UPVALUE, (uint16_t)slot);
        }
        emitOp(OP_POP);
    }
    current_->locals.resize(current_->locals.size() - count);
    current_->scopeDepth--;
}

int Compiler::addLocal(const std::string& name, bool isConst) {
    if ((int)current_->locals.size() >= 256)
        error("Too many local variables (max 256)");
    for (int i = (int)current_->locals.size() - 1; i >= 0; i--) {
        if (current_->locals[i].depth < current_->scopeDepth) break;
        if (current_->locals[i].name == name)
            error("Variable '" + name + "' already declared in this scope");
    }
    int slot = (int)current_->locals.size();
    current_->locals.push_back({name, current_->scopeDepth, isConst});
    return slot;
}

int Compiler::resolveLocal(const std::string& name) {
    for (int i = (int)current_->locals.size() - 1; i >= 0; i--) {
        if (current_->locals[i].name == name) return i;
    }
    return -1;
}

int Compiler::addUpvalue(Compiler::CompilerState* compiler, uint8_t index, bool isLocal) {
    for (int i = 0; i < (int)compiler->upvalues.size(); i++) {
        auto& u = compiler->upvalues[i];
        if (u.index == index && u.isLocal == isLocal) return i;
    }
    if ((int)compiler->upvalues.size() >= 256) error("Too many closure upvalues");
    compiler->upvalues.push_back({index, isLocal});
    compiler->function->upvalueCount = (int)compiler->upvalues.size();
    return (int)compiler->upvalues.size() - 1;
}

int Compiler::resolveUpvalue(const std::string& name) {
    if (current_->function->chunk->name == "(script)") return -1;
    auto* enclosing = compilerStack_.size() >= 2 ? compilerStack_[compilerStack_.size() - 2] : nullptr;
    if (!enclosing) return -1;

    int local = -1;
    for (int i = (int)enclosing->locals.size() - 1; i >= 0; i--) {
        if (enclosing->locals[i].name == name) { local = i; break; }
    }
    if (local >= 0) {
        enclosing->locals[local].isConst = false;
        enclosing->locals[local].captured = true;
        return addUpvalue(current_, (uint8_t)local, true);
    }
    return -1;
}
void Compiler::emitByte(uint8_t byte) { currentChunk().writeByte(byte, currentLine_); }

void Compiler::emitOp(uint8_t op) { currentChunk().writeOp(op, currentLine_); }

void Compiler::emitOpOperand16(uint8_t op, uint16_t operand) {
    currentChunk().writeOpOperand16(op, operand, -1);
}

int Compiler::emitJump(uint8_t op) { return currentChunk().writeJump(op, -1); }

void Compiler::patchJump(int offset) { currentChunk().patchJump(offset); }

void Compiler::emitLoop(int loopStart) { currentChunk().writeLoop(loopStart, -1); }

uint16_t Compiler::addConstant(const Value& value) {
    return (uint16_t)currentChunk().addConstant(value);
}

void Compiler::emitConstant(const Value& value) {
    emitOpOperand16(OP_CONSTANT, addConstant(value));
}

void Compiler::compileStatements(const std::vector<StmtPtr>& stmts) {
    for (auto& s : stmts) compileStatement(s);
}

void Compiler::compileStatement(StmtPtr stmt) {
    currentLine_ = stmt->line;
    struct Visitor {
        Compiler& c;
        void operator()(ExprStmt& s) { c.compileExprStmt(s); }
        void operator()(VarDecl& s) { c.compileVarDecl(s); }
        void operator()(ReturnStmt& s) { c.compileReturn(s); }
        void operator()(IfStmt& s) { c.compileIf(s); }
        void operator()(WhileStmt& s) { c.compileWhile(s); }
        void operator()(ForStmt& s) { c.compileFor(s); }
        void operator()(ForInStmt& s) { c.compileForIn(s); }
        void operator()(BlockStmt& s) { c.compileBlock(s); }
        void operator()(FnDecl& s) { c.compileFnDecl(s); }
        void operator()(GenDecl& s) { c.compileGenDecl(s); }
        void operator()(ClassDecl& s) { c.compileClassDecl(s); }
        void operator()(StructDecl& s) { c.compileStructDecl(s); }
        void operator()(ImplDecl& s) { c.compileImplDecl(s); }
        void operator()(EnumDecl& s) { c.compileEnumDecl(s); }
        void operator()(TryStmt& s) { c.compileTryCatch(s); }
        void operator()(ExternFnDecl& s) { c.compileExternFn(s); }
        void operator()(ImportStmt& s) { c.compileImport(s); }
        void operator()(BreakStmt& s) { c.compileBreak(s); }
        void operator()(ContinueStmt& s) { c.compileContinue(s); }
    };
    std::visit(Visitor{*this}, stmt->node);
}

void Compiler::compileExprStmt(ExprStmt& stmt) {
    compileExpression(stmt.expression);
    emitOp(replMode_ ? OP_PRINT : OP_POP);
}

void Compiler::compileBlock(BlockStmt& stmt) {
    beginScope();
    compileStatements(stmt.statements);
    endScope();
}

void Compiler::compileBreak(BreakStmt&) {
    if (!currentLoop_) error("break outside of loop");
    currentLoop_->breakJumps.push_back(emitJump(OP_JUMP));
}

void Compiler::compileContinue(ContinueStmt&) {
    if (!currentLoop_) error("continue outside of loop");
    emitLoop(currentLoop_->loopStart);
}

void Compiler::compileVarDecl(VarDecl& decl) {
    if (current_->scopeDepth > 0) {
        int slot = addLocal(decl.name, decl.isConst);
        if (decl.initializer) {
            compileExpression(decl.initializer);
        } else {
            emitOp(OP_NIL);
        }
    } else {
        if (decl.isConst) constGlobals_.insert(decl.name);
        if (decl.initializer) {
            compileExpression(decl.initializer);
        } else {
            emitOp(OP_NIL);
        }
        emitOpOperand16(OP_DEFINE_GLOBAL, addConstant(Value::obj(std::make_shared<ObjString>(decl.name))));
    }
}

void Compiler::compileFnDecl(FnDecl& decl) {
    auto fn = std::make_shared<ObjFunction>();
    fn->name = decl.name;
    fn->arity = (int)decl.params.size();

    auto* prev = current_;
    pushCompiler(fn);
    current_->function->arity = fn->arity;
    current_->function->name = decl.name;

    beginScope();
    current_->locals[0].name = "this";
    for (int i = 0; i < (int)decl.params.size(); i++) {
        addLocal(decl.params[i], false);
    }
    compileStatements(decl.body);
    emitOp(OP_NIL);
    emitOp(OP_RETURN);
    endScope();

    auto* state = popCompiler();
    auto compiledFn = state->function;
    auto savedUpvalues = state->upvalues;
    current_ = prev;

    emitOpOperand16(OP_CLOSURE, addConstant(Value::obj(compiledFn)));
    for (auto& uv : savedUpvalues) {
        emitByte(uv.isLocal ? 1 : 0);
        emitByte(uv.index);
    }

    if (current_->scopeDepth > 0) {
        emitOpOperand16(OP_SET_LOCAL, (uint16_t)resolveLocal(decl.name));
    } else {
        emitOpOperand16(OP_DEFINE_GLOBAL, addConstant(Value::obj(std::make_shared<ObjString>(decl.name))));
    }
    delete state;
}

void Compiler::compileGenDecl(GenDecl& decl) {
    auto fn = std::make_shared<ObjFunction>();
    fn->name = decl.name;
    fn->arity = (int)decl.params.size();
    fn->isGenerator = true;

    auto* prev = current_;
    pushCompiler(fn);
    current_->function->arity = fn->arity;
    current_->function->name = decl.name;

    beginScope();
    current_->locals[0].name = "this";
    for (int i = 0; i < (int)decl.params.size(); i++) {
        addLocal(decl.params[i], false);
    }
    compileStatements(decl.body);
    fn->localCount = (int)current_->locals.size();
    emitOp(OP_NIL);
    emitOp(OP_RETURN);
    endScope();

    auto* state = popCompiler();
    auto compiledFn = state->function;
    auto savedUpvalues = state->upvalues;
    current_ = prev;

    emitOpOperand16(OP_CLOSURE, addConstant(Value::obj(compiledFn)));
    for (auto& uv : savedUpvalues) {
        emitByte(uv.isLocal ? 1 : 0);
        emitByte(uv.index);
    }

    if (current_->scopeDepth > 0) {
        emitOpOperand16(OP_SET_LOCAL, (uint16_t)resolveLocal(decl.name));
    } else {
        emitOpOperand16(OP_DEFINE_GLOBAL, addConstant(Value::obj(std::make_shared<ObjString>(decl.name))));
    }
    delete state;
}

void Compiler::compileClassDecl(ClassDecl& decl) {
    uint16_t nameIdx = addConstant(Value::obj(std::make_shared<ObjString>(decl.name)));

    if (current_->scopeDepth > 0) {
        addLocal(decl.name, false);
        emitOpOperand16(OP_CLASS, nameIdx);
        emitOpOperand16(OP_SET_LOCAL, (uint16_t)resolveLocal(decl.name));
    } else {
        emitOpOperand16(OP_CLASS, nameIdx);
        emitOpOperand16(OP_DEFINE_GLOBAL, nameIdx);
    }

    auto* classState = new Compiler::ClassCompilerState();
    classState->enclosing = currentClass_;
    currentClass_ = classState;

    if (decl.superclass) {
        if (auto* id = std::get_if<Identifier>(&decl.superclass->node)) {
            currentClass_->hasSuperclass = true;
            if (current_->scopeDepth > 0) {
                emitOpOperand16(OP_GET_LOCAL, (uint16_t)resolveLocal(decl.name));
            } else {
                emitOpOperand16(OP_GET_GLOBAL, nameIdx);
            }
            compileExpression(decl.superclass);
            emitOp(OP_INHERIT);
        }
    } else {
        if (current_->scopeDepth > 0) {
            emitOpOperand16(OP_GET_LOCAL, (uint16_t)resolveLocal(decl.name));
        } else {
            emitOpOperand16(OP_GET_GLOBAL, nameIdx);
        }
    }

    for (auto& method : decl.methods) {
        auto fn = std::make_shared<ObjFunction>();
        fn->name = method.name;
        fn->arity = (int)method.params.size();

        auto* prev = current_;
        pushCompiler(fn);
        current_->function->arity = fn->arity;
        current_->function->name = method.name;

        beginScope();
        current_->locals[0].name = "this";
        for (int i = 0; i < (int)method.params.size(); i++) {
            addLocal(method.params[i], false);
        }
        compileStatements(method.body);
        if (method.name == "init") {
            emitOpOperand16(OP_GET_LOCAL, 0);
        } else {
            emitOp(OP_NIL);
        }
        emitOp(OP_RETURN);
        endScope();

        auto compiledFn = popCompiler()->function;
        current_ = prev;

        emitOpOperand16(OP_CLOSURE, addConstant(Value::obj(compiledFn)));
        uint16_t methodNameIdx = addConstant(Value::obj(std::make_shared<ObjString>(method.name)));
        emitOpOperand16(OP_METHOD, methodNameIdx);
    }

    emitOp(OP_POP);
    auto* prev = classState->enclosing;
    delete currentClass_;
    currentClass_ = prev;
}

void Compiler::compileStructDecl(StructDecl& decl) {
    std::vector<FnDecl> methods;
    FnDecl initDecl;
    initDecl.name = "init";
    initDecl.params = decl.fields;
    for (int i = 0; i < (int)decl.fields.size(); i++) {
        auto object = std::make_shared<Expr>();
        object->node = ThisLiteral{};
        auto member = decl.fields[i];
        auto value = std::make_shared<Expr>();
        value->node = Identifier{decl.fields[i]};
        auto assign = std::make_shared<Expr>();
        assign->node = AssignMemberExpr{object, member, value};
        auto stmt = std::make_shared<Stmt>();
        stmt->node = ExprStmt{assign};
        initDecl.body.push_back(stmt);
    }
    methods.push_back(initDecl);
    ClassDecl classDecl{decl.name, methods, nullptr};
    compileClassDecl(classDecl);
}

void Compiler::compileImplDecl(ImplDecl& decl) {
    uint16_t nameIdx = addConstant(Value::obj(std::make_shared<ObjString>(decl.className)));

    auto* classState = new Compiler::ClassCompilerState();
    classState->enclosing = currentClass_;
    classState->hasSuperclass = false;
    currentClass_ = classState;

    if (current_->scopeDepth > 0) {
        emitOpOperand16(OP_GET_LOCAL, (uint16_t)resolveLocal(decl.className));
    } else {
        emitOpOperand16(OP_GET_GLOBAL, nameIdx);
    }

    for (auto& method : decl.methods) {
        auto fn = std::make_shared<ObjFunction>();
        fn->name = method.name;
        fn->arity = (int)method.params.size();

        auto* prev = current_;
        pushCompiler(fn);
        current_->function->arity = fn->arity;
        current_->function->name = method.name;

        beginScope();
        current_->locals[0].name = "this";
        for (int i = 0; i < (int)method.params.size(); i++) {
            addLocal(method.params[i], false);
        }
        compileStatements(method.body);
        if (method.name == "init") {
            emitOpOperand16(OP_GET_LOCAL, 0);
        } else {
            emitOp(OP_NIL);
        }
        emitOp(OP_RETURN);
        endScope();

        auto compiledFn = popCompiler()->function;
        current_ = prev;

        emitOpOperand16(OP_CLOSURE, addConstant(Value::obj(compiledFn)));
        uint16_t methodNameIdx = addConstant(Value::obj(std::make_shared<ObjString>(method.name)));
        emitOpOperand16(OP_METHOD, methodNameIdx);
    }

    emitOp(OP_POP);
    auto* prevClass = classState->enclosing;
    delete currentClass_;
    currentClass_ = prevClass;
}

void Compiler::compileEnumDecl(EnumDecl& decl) {
    for (auto& variant : decl.variants) {
        uint16_t valIdx = addConstant(Value::obj(std::make_shared<ObjString>(variant)));
        uint16_t nameIdx = addConstant(Value::obj(std::make_shared<ObjString>(variant)));
        emitConstant(Value::obj(std::make_shared<ObjString>(variant)));
        emitOpOperand16(OP_DEFINE_GLOBAL, nameIdx);
    }
}

void Compiler::compileReturn(ReturnStmt& stmt) {
    if (stmt.value) {
        compileExpression(stmt.value);
    } else {
        emitOp(OP_NIL);
    }
    emitOp(OP_RETURN);
}

void Compiler::compileIf(IfStmt& stmt) {
    compileExpression(stmt.condition);
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitOp(OP_POP);
    beginScope();
    compileStatements(stmt.thenBranch);
    endScope();
    if (!stmt.elseBranch.empty()) {
        int elseJump = emitJump(OP_JUMP);
        patchJump(thenJump);
        emitOp(OP_POP);
        beginScope();
        compileStatements(stmt.elseBranch);
        endScope();
        patchJump(elseJump);
    } else {
        int endJump = emitJump(OP_JUMP);
        patchJump(thenJump);
        emitOp(OP_POP);
        patchJump(endJump);
    }
}

void Compiler::compileWhile(WhileStmt& stmt) {
    LoopState loop;
    loop.loopStart = (int)currentChunk().code.size();
    LoopState* prev = currentLoop_;
    currentLoop_ = &loop;
    beginScope();
    compileExpression(stmt.condition);
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitOp(OP_POP);
    compileStatements(stmt.body);
    endScope();
    emitLoop(loop.loopStart);
    patchJump(exitJump);
    emitOp(OP_POP);
    for (int bj : loop.breakJumps) patchJump(bj);
    currentLoop_ = prev;
}

void Compiler::compileFor(ForStmt& stmt) {
    LoopState loop;
    loop.loopStart = (int)currentChunk().code.size();
    LoopState* prev = currentLoop_;
    currentLoop_ = &loop;
    beginScope();
    if (stmt.initializer) compileStatement(stmt.initializer);
    loop.loopStart = (int)currentChunk().code.size();
    if (stmt.condition) {
        compileExpression(stmt.condition);
        int exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitOp(OP_POP);
        compileStatements(stmt.body);
        if (stmt.increment) { compileExpression(stmt.increment); emitOp(OP_POP); }
        emitLoop(loop.loopStart);
        patchJump(exitJump);
        emitOp(OP_POP);
    } else {
        compileStatements(stmt.body);
        if (stmt.increment) { compileExpression(stmt.increment); emitOp(OP_POP); }
        emitLoop(loop.loopStart);
    }
    endScope();
    for (int bj : loop.breakJumps) patchJump(bj);
    currentLoop_ = prev;
}

void Compiler::compileForIn(ForInStmt& stmt) {
    static int forInCounter = 0;
    int id = forInCounter++;

    beginScope();

    std::string iterName = "__iter_" + std::to_string(id);
    std::string idxName = "__idx_" + std::to_string(id);

    compileExpression(stmt.iterable);
    int iterSlot = addLocal(iterName, false);

    emitConstant(Value::integer(0));
    int idxSlot = addLocal(idxName, false);

    emitOp(OP_NIL);
    int varSlot = addLocal(stmt.variable, false);

    int loopStart = (int)currentChunk().code.size();

    emitOpOperand16(OP_GET_LOCAL, (uint16_t)idxSlot);
    emitOpOperand16(OP_GET_LOCAL, (uint16_t)iterSlot);
    emitOp(OP_INDEX_LEN);
    emitOp(OP_LESS);
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitOp(OP_POP);

    emitOpOperand16(OP_GET_LOCAL, (uint16_t)iterSlot);
    emitOpOperand16(OP_GET_LOCAL, (uint16_t)idxSlot);
    emitOp(OP_INDEX);
    emitOpOperand16(OP_SET_LOCAL, (uint16_t)varSlot);

    compileStatements(stmt.body);

    emitOpOperand16(OP_GET_LOCAL, (uint16_t)idxSlot);
    emitConstant(Value::integer(1));
    emitOp(OP_ADD);
    emitOpOperand16(OP_SET_LOCAL, (uint16_t)idxSlot);

    emitLoop(loopStart);
    patchJump(exitJump);
    emitOp(OP_POP);

    endScope();
}

void Compiler::compileTryCatch(TryStmt& stmt) {
    int tryJump = emitJump(OP_TRY);
    beginScope();
    compileStatements(stmt.body);
    endScope();
    emitOp(OP_END_TRY);
    int endJump = emitJump(OP_JUMP);
    patchJump(tryJump);
    beginScope();
    addLocal(stmt.catchVar, false);
    emitOpOperand16(OP_SET_LOCAL, (uint16_t)(current_->locals.size() - 1));
    emitOp(OP_POP);
    compileStatements(stmt.catchBody);
    endScope();
    patchJump(endJump);
}

void Compiler::compileExternFn(ExternFnDecl& decl) {
    auto nameObj = std::make_shared<ObjString>(decl.name);
    auto libObj = std::make_shared<ObjString>(decl.library);
    uint16_t nameIdx = addConstant(Value::obj(nameObj));
    uint16_t libIdx = addConstant(Value::obj(libObj));
    emitOpOperand16(OP_IMPORT, nameIdx);
    emitByte((uint8_t)decl.params.size());
    emitByte(libIdx & 0xFF);
    if (current_->scopeDepth > 0) {
        emitOpOperand16(OP_SET_LOCAL, (uint16_t)resolveLocal(decl.name));
    } else {
        emitOpOperand16(OP_DEFINE_GLOBAL, nameIdx);
    }
}

void Compiler::compileImport(ImportStmt& decl) {
    auto nameObj = std::make_shared<ObjString>(decl.moduleName);
    uint16_t nameIdx = addConstant(Value::obj(nameObj));
    emitOpOperand16(OP_IMPORT, nameIdx);
    emitByte(0);
    emitByte(0);
    if (current_->scopeDepth > 0) {
        addLocal(decl.moduleName, false);
        emitOpOperand16(OP_SET_LOCAL, (uint16_t)resolveLocal(decl.moduleName));
    } else {
        emitOpOperand16(OP_DEFINE_GLOBAL, nameIdx);
    }
}

// ---- Expression Compilation ----

void Compiler::compileExpression(ExprPtr expr) {
    currentLine_ = expr->line;
    struct Visitor {
        Compiler& c;
        void operator()(IntegerLiteral& e) { c.compileIntegerLiteral(e); }
        void operator()(FloatLiteral& e) { c.compileFloatLiteral(e); }
        void operator()(CharLiteral& e) { c.compileCharLiteral(e); }
        void operator()(StringLiteral& e) { c.compileStringLiteral(e); }
        void operator()(BoolLiteral& e) { c.compileBoolLiteral(e); }
        void operator()(NilLiteral& e) { c.compileNilLiteral(); }
        void operator()(ThisLiteral& e) { c.compileThisLiteral(); }
        void operator()(Identifier& e) { c.compileIdentifier(e); }
        void operator()(BinaryExpr& e) { c.compileBinary(e); }
        void operator()(UnaryExpr& e) { c.compileUnary(e); }
        void operator()(TernaryExpr& e) { c.compileTernary(e); }
        void operator()(CallExpr& e) { c.compileCall(e); }
        void operator()(IndexExpr& e) { c.compileIndex(e); }
        void operator()(MemberExpr& e) { c.compileMember(e); }
        void operator()(ArrayLiteral& e) { c.compileArrayLiteral(e); }
        void operator()(MapLiteral& e) { c.compileMapLiteral(e); }
        void operator()(FnExpr& e) { c.compileFnExpr(e); }
        void operator()(SuperExpr& e) { c.compileSuperExpr(e); }
        void operator()(StringInterpExpr& e) { c.compileStringInterp(e); }
        void operator()(MatchExpr& e) { c.compileMatchExpr(e); }
        void operator()(ThrowExpr& e) { c.compileThrowExpr(e); }
        void operator()(YieldExpr& e) { c.compileYieldExpr(e); }
        void operator()(AssignExpr& e) { c.compileAssign(e); }
        void operator()(AssignMemberExpr& e) { c.compileAssignMember(e); }
        void operator()(AssignIndexExpr& e) { c.compileAssignIndex(e); }
    };
    std::visit(Visitor{*this}, expr->node);
}

void Compiler::compileIntegerLiteral(IntegerLiteral& e) {
    emitConstant(Value::integer(e.value));
}

void Compiler::compileCharLiteral(CharLiteral& e) {
    emitConstant(Value::integer(static_cast<int64_t>(e.value)));
}

void Compiler::compileFloatLiteral(FloatLiteral& e) {
    emitConstant(Value::floating(e.value));
}

void Compiler::compileStringLiteral(StringLiteral& e) {
    emitConstant(Value::obj(std::make_shared<ObjString>(e.value)));
}

void Compiler::compileBoolLiteral(BoolLiteral& e) {
    emitOp(e.value ? OP_TRUE : OP_FALSE);
}

void Compiler::compileNilLiteral() { emitOp(OP_NIL); }

void Compiler::compileThisLiteral() {
    int slot = resolveLocal("this");
    if (slot >= 0) {
        emitOpOperand16(OP_GET_LOCAL, (uint16_t)slot);
    } else {
        error("'this' used outside of a class method");
    }
}

void Compiler::compileIdentifier(Identifier& e) {
    int slot = resolveLocal(e.name);
    if (slot >= 0) {
        emitOpOperand16(OP_GET_LOCAL, (uint16_t)slot);
        return;
    }
    int upval = resolveUpvalue(e.name);
    if (upval >= 0) {
        emitOp(OP_GET_UPVALUE);
        emitByte((uint8_t)upval);
        return;
    }
    emitOpOperand16(OP_GET_GLOBAL, addConstant(Value::obj(std::make_shared<ObjString>(e.name))));
}

void Compiler::compileBinary(BinaryExpr& e) {
    if (tryFoldBinary(e)) return;
    if (e.op.type == TokenType::AND) {
        compileExpression(e.left);
        int elseJump = emitJump(OP_JUMP_IF_FALSE);
        emitOp(OP_POP);
        compileExpression(e.right);
        patchJump(elseJump);
        return;
    }
    if (e.op.type == TokenType::OR) {
        compileExpression(e.left);
        int elseJump = emitJump(OP_JUMP_IF_TRUE);
        emitOp(OP_POP);
        compileExpression(e.right);
        patchJump(elseJump);
        return;
    }
    compileExpression(e.left);
    compileExpression(e.right);
    switch (e.op.type) {
        case TokenType::PLUS: emitOp(OP_ADD); break;
        case TokenType::MINUS: emitOp(OP_SUBTRACT); break;
        case TokenType::STAR: emitOp(OP_MULTIPLY); break;
        case TokenType::SLASH: emitOp(OP_DIVIDE); break;
        case TokenType::PERCENT: emitOp(OP_MODULO); break;
        case TokenType::AMPERSAND: emitOp(OP_BITWISE_AND); break;
        case TokenType::BAR: emitOp(OP_BITWISE_OR); break;
        case TokenType::CARET: emitOp(OP_BITWISE_XOR); break;
        case TokenType::EQUAL_EQUAL: emitOp(OP_EQUAL); break;
        case TokenType::BANG_EQUAL: emitOp(OP_NOT_EQUAL); break;
        case TokenType::LESS: emitOp(OP_LESS); break;
        case TokenType::LESS_EQUAL: emitOp(OP_LESS_EQUAL); break;
        case TokenType::GREATER: emitOp(OP_GREATER); break;
        case TokenType::GREATER_EQUAL: emitOp(OP_GREATER_EQUAL); break;
        default: error("Invalid binary operator '" + e.op.value + "'");
    }
}

void Compiler::compileUnary(UnaryExpr& e) {
    compileExpression(e.operand);
    switch (e.op.type) {
        case TokenType::MINUS: emitOp(OP_NEGATE); break;
        case TokenType::NOT: emitOp(OP_NOT); break;
        default: error("Invalid unary operator");
    }
}

void Compiler::compileTernary(TernaryExpr& e) {
    compileExpression(e.condition);
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    emitOp(OP_POP);
    compileExpression(e.thenExpr);
    int endJump = emitJump(OP_JUMP);
    patchJump(elseJump);
    emitOp(OP_POP);
    compileExpression(e.elseExpr);
    patchJump(endJump);
}

void Compiler::compileCall(CallExpr& e) {
    compileExpression(e.callee);
    for (auto& arg : e.arguments) compileExpression(arg);
    emitOp(OP_CALL);
    emitByte((uint8_t)e.arguments.size());
}

void Compiler::compileIndex(IndexExpr& e) {
    compileExpression(e.object);
    compileExpression(e.index);
    emitOp(OP_INDEX);
}

void Compiler::compileMember(MemberExpr& e) {
    compileExpression(e.object);
    emitOpOperand16(OP_GET_PROPERTY, addConstant(Value::obj(std::make_shared<ObjString>(e.member))));
}

void Compiler::compileArrayLiteral(ArrayLiteral& e) {
    for (auto& elem : e.elements) compileExpression(elem);
    emitOpOperand16(OP_ARRAY, (uint16_t)e.elements.size());
}

void Compiler::compileMapLiteral(MapLiteral& e) {
    for (auto& entry : e.entries) {
        compileExpression(entry.key);
        compileExpression(entry.value);
    }
    emitOpOperand16(OP_MAP, (uint16_t)e.entries.size());
}

void Compiler::compileFnExpr(FnExpr& e) {
    auto fn = std::make_shared<ObjFunction>();
    fn->name = "<lambda>";
    fn->arity = (int)e.params.size();

    auto* prev = current_;
    pushCompiler(fn);
    current_->function->arity = fn->arity;
    current_->function->name = "<lambda>";

    beginScope();
    current_->locals[0].name = "this";
    for (auto& p : e.params) addLocal(p, false);
    compileStatements(e.body);
    emitOp(OP_NIL);
    emitOp(OP_RETURN);
    endScope();

    auto* state = popCompiler();
    auto compiledFn = state->function;
    auto savedUpvalues = state->upvalues;
    current_ = prev;

    emitOpOperand16(OP_CLOSURE, addConstant(Value::obj(compiledFn)));
    for (auto& uv : savedUpvalues) {
        emitByte(uv.isLocal ? 1 : 0);
        emitByte(uv.index);
    }
    delete state;
}

void Compiler::compileSuperExpr(SuperExpr& e) {
    if (!currentClass_ || !currentClass_->hasSuperclass)
        error("'super' used outside of a class with superclass");
    int thisSlot = resolveLocal("this");
    emitOpOperand16(OP_GET_LOCAL, (uint16_t)thisSlot);
    emitOpOperand16(OP_GET_SUPER, addConstant(Value::obj(std::make_shared<ObjString>(e.method))));
}

void Compiler::compileStringInterp(StringInterpExpr& e) {
    if (e.parts.empty()) {
        emitConstant(Value::obj(std::make_shared<ObjString>("")));
        return;
    }
    emitConstant(Value::obj(std::make_shared<ObjString>("")));
    for (auto& part : e.parts) {
        if (part.isExpr) {
            emitOpOperand16(OP_GET_GLOBAL, addConstant(Value::obj(std::make_shared<ObjString>("str"))));
            compileExpression(part.expression);
            emitOp(OP_CALL);
            emitByte(1);
        } else {
            emitConstant(Value::obj(std::make_shared<ObjString>(part.text)));
        }
        emitOp(OP_ADD);
    }
}

void Compiler::compileMatchExpr(MatchExpr& e) {
    int savedLocalsCount = (int)current_->locals.size();

    compileExpression(e.value);
    addLocal("_match_val", true);
    int matchSlot = (int)current_->locals.size() - 1;

    std::vector<int> endJumps;

    for (size_t i = 0; i < e.cases.size(); i++) {
        auto& mc = e.cases[i];

        auto compileBody = [&](const std::vector<StmtPtr>& body) {
            for (size_t j = 0; j < body.size(); j++) {
                if (j < body.size() - 1) {
                    compileStatement(body[j]);
                } else {
                    if (auto* exprStmt = std::get_if<ExprStmt>(&body[j]->node)) {
                        compileExpression(exprStmt->expression);
                    } else {
                        compileStatement(body[j]);
                        emitOp(OP_NIL);
                    }
                }
            }
        };

        if (mc.pattern.type == MatchPattern::WILDCARD) {
            emitOpOperand16(OP_GET_LOCAL, (uint16_t)matchSlot);
            int varSlot = addLocal("_wildcard", false);
            emitOpOperand16(OP_SET_LOCAL, (uint16_t)varSlot);
            emitOp(OP_POP);
            compileBody(mc.body);
            break;
        } else if (mc.pattern.type == MatchPattern::IDENTIFIER) {
            emitOpOperand16(OP_GET_LOCAL, (uint16_t)matchSlot);
            int varSlot = addLocal(mc.pattern.name, false);
            emitOpOperand16(OP_SET_LOCAL, (uint16_t)varSlot);
            emitOp(OP_POP);
            compileBody(mc.body);
            break;
        } else {
            emitOpOperand16(OP_GET_LOCAL, (uint16_t)matchSlot);
            compileExpression(mc.pattern.literal);
            emitOp(OP_EQUAL);
            int jumpElse = emitJump(OP_JUMP_IF_FALSE);
            emitOp(OP_POP);
            compileBody(mc.body);
            if (i < e.cases.size() - 1) {
                endJumps.push_back(emitJump(OP_JUMP));
            }
            patchJump(jumpElse);
            emitOp(OP_POP);
        }
    }

    for (int j : endJumps) patchJump(j);
    current_->locals.resize(savedLocalsCount);
}

void Compiler::compileThrowExpr(ThrowExpr& e) {
    compileExpression(e.value);
    emitOp(OP_THROW);
}

void Compiler::compileYieldExpr(YieldExpr& e) {
    if (e.value) {
        compileExpression(e.value);
    } else {
        emitOp(OP_NIL);
    }
    emitOp(OP_YIELD);
    emitOp(OP_NIL);
}

void Compiler::compileAssign(AssignExpr& e) {
    int slot = resolveLocal(e.name);
    if (slot >= 0) {
        if (current_->locals[slot].isConst) {
            error("Cannot reassign constant variable '" + e.name + "'");
        }
        compileExpression(e.value);
        emitOpOperand16(OP_SET_LOCAL, (uint16_t)slot);
    } else {
        int upval = resolveUpvalue(e.name);
        if (upval >= 0) {
            compileExpression(e.value);
            emitOp(OP_SET_UPVALUE);
            emitByte((uint8_t)upval);
        } else {
            if (constGlobals_.count(e.name)) {
                error("Cannot reassign constant variable '" + e.name + "'");
            }
            compileExpression(e.value);
            emitOpOperand16(OP_SET_GLOBAL, addConstant(Value::obj(std::make_shared<ObjString>(e.name))));
        }
    }
}

void Compiler::compileAssignMember(AssignMemberExpr& e) {
    compileExpression(e.object);
    compileExpression(e.value);
    emitOpOperand16(OP_SET_PROPERTY, addConstant(Value::obj(std::make_shared<ObjString>(e.member))));
}

void Compiler::compileAssignIndex(AssignIndexExpr& e) {
    compileExpression(e.object);
    compileExpression(e.index);
    compileExpression(e.value);
    emitOp(OP_SET_INDEX);
}

bool Compiler::isLiteralConstant(ExprPtr expr) {
    if (!expr) return false;
    return std::holds_alternative<IntegerLiteral>(expr->node) ||
           std::holds_alternative<FloatLiteral>(expr->node) ||
           std::holds_alternative<BoolLiteral>(expr->node) ||
           std::holds_alternative<NilLiteral>(expr->node) ||
           std::holds_alternative<StringLiteral>(expr->node);
}

bool Compiler::tryFoldBinary(BinaryExpr& e) {
    if (!isLiteralConstant(e.left) || !isLiteralConstant(e.right)) return false;
    if (std::holds_alternative<IntegerLiteral>(e.left->node) && std::holds_alternative<IntegerLiteral>(e.right->node)) {
        long long a = std::get<IntegerLiteral>(e.left->node).value;
        long long b = std::get<IntegerLiteral>(e.right->node).value;
        long long result;
        switch (e.op.type) {
            case TokenType::PLUS: result = a + b; break;
            case TokenType::MINUS: result = a - b; break;
            case TokenType::STAR: result = a * b; break;
            case TokenType::SLASH: if (b == 0) return false; result = a / b; break;
            case TokenType::PERCENT: if (b == 0) return false; result = a % b; break;
            case TokenType::EQUAL_EQUAL: emitConstant(Value::boolean(a == b)); return true;
            case TokenType::BANG_EQUAL: emitConstant(Value::boolean(a != b)); return true;
            case TokenType::LESS: emitConstant(Value::boolean(a < b)); return true;
            case TokenType::LESS_EQUAL: emitConstant(Value::boolean(a <= b)); return true;
            case TokenType::GREATER: emitConstant(Value::boolean(a > b)); return true;
            case TokenType::GREATER_EQUAL: emitConstant(Value::boolean(a >= b)); return true;
            default: return false;
        }
        emitConstant(Value::integer(result));
        return true;
    }
    if (std::holds_alternative<FloatLiteral>(e.left->node) && std::holds_alternative<FloatLiteral>(e.right->node)) {
        double a = std::get<FloatLiteral>(e.left->node).value;
        double b = std::get<FloatLiteral>(e.right->node).value;
        double result;
        switch (e.op.type) {
            case TokenType::PLUS: result = a + b; break;
            case TokenType::MINUS: result = a - b; break;
            case TokenType::STAR: result = a * b; break;
            case TokenType::SLASH: if (b == 0.0) return false; result = a / b; break;
            case TokenType::EQUAL_EQUAL: emitConstant(Value::boolean(a == b)); return true;
            case TokenType::BANG_EQUAL: emitConstant(Value::boolean(a != b)); return true;
            case TokenType::LESS: emitConstant(Value::boolean(a < b)); return true;
            case TokenType::LESS_EQUAL: emitConstant(Value::boolean(a <= b)); return true;
            case TokenType::GREATER: emitConstant(Value::boolean(a > b)); return true;
            case TokenType::GREATER_EQUAL: emitConstant(Value::boolean(a >= b)); return true;
            default: return false;
        }
        emitConstant(Value::floating(result));
        return true;
    }
    if (e.op.type == TokenType::PLUS &&
        std::holds_alternative<StringLiteral>(e.left->node) && std::holds_alternative<StringLiteral>(e.right->node)) {
        auto& a = std::get<StringLiteral>(e.left->node).value;
        auto& b = std::get<StringLiteral>(e.right->node).value;
        emitConstant(Value::obj(std::make_shared<ObjString>(a + b)));
        return true;
    }
    return false;
}

std::shared_ptr<ObjFunction> Compiler::compile(const std::vector<StmtPtr>& program) {
    hadError_ = false;
    errorMessage_.clear();
    auto fn = std::make_shared<ObjFunction>();
    fn->name = "(script)";
    pushCompiler(fn);
    current_->scopeDepth = 0;
    compileStatements(program);
    emitOp(OP_NIL);
    emitOp(OP_RETURN);
    auto result = popCompiler()->function;
    return result;
}
