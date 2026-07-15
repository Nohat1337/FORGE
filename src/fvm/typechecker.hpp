#pragma once

#include "runtime.hpp"
#include "../ast.hpp"
#include "../parser.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>

namespace forge::typecheck {

enum class TypeKind {
    NIL,
    BOOL,
    INT,
    FLOAT,
    STRING,
    ARRAY,
    MAP,
    FUNCTION,
    CLASS,
    ANY,
    UNKNOWN
};

struct Type {
    TypeKind kind = TypeKind::UNKNOWN;
    std::string name;  // For classes, function signatures
    std::vector<Type> params;  // For arrays, maps, functions
    Type() = default;
    Type(TypeKind k) : kind(k) {}
    Type(TypeKind k, const std::string& n) : kind(k), name(n) {}
    
    static Type nil() { return Type(TypeKind::NIL); }
    static Type boolean() { return Type(TypeKind::BOOL); }
    static Type integer() { return Type(TypeKind::INT); }
    static Type floating() { return Type(TypeKind::FLOAT); }
    static Type string() { return Type(TypeKind::STRING); }
    static Type array(const Type& elem) { 
        Type t(TypeKind::ARRAY); t.params.push_back(elem); return t; 
    }
    static Type map(const Type& key, const Type& val) { 
        Type t(TypeKind::MAP); t.params = {key, val}; return t; 
    }
    static Type function(const std::vector<Type>& args, const Type& ret) {
        Type t(TypeKind::FUNCTION); t.params = args; t.name = ret.kind == TypeKind::UNKNOWN ? "" : ret.name; return t;
    }
    static Type classType(const std::string& name) { return Type(TypeKind::CLASS, name); }
    static Type any() { return Type(TypeKind::ANY); }
    static Type unknown() { return Type(TypeKind::UNKNOWN); }
    
    bool isNumeric() const { return kind == TypeKind::INT || kind == TypeKind::FLOAT; }
    bool equals(const Type& other) const {
        if (kind == TypeKind::ANY || other.kind == TypeKind::ANY) return true;
        if (kind != other.kind) return false;
        if (kind == TypeKind::CLASS) return name == other.name;
        if (kind == TypeKind::ARRAY || kind == TypeKind::MAP || kind == TypeKind::FUNCTION) {
            if (params.size() != other.params.size()) return false;
            for (size_t i = 0; i < params.size(); i++) {
                if (!params[i].equals(other.params[i])) return false;
            }
            return true;
        }
        return true;
    }
    std::string toString() const {
        switch (kind) {
            case TypeKind::NIL: return "nil";
            case TypeKind::BOOL: return "bool";
            case TypeKind::INT: return "int";
            case TypeKind::FLOAT: return "float";
            case TypeKind::STRING: return "string";
            case TypeKind::ARRAY: return "[" + (params.empty() ? "any" : params[0].toString()) + "]";
            case TypeKind::MAP: return "{ " + (params.size() >= 2 ? params[0].toString() + " : " + params[1].toString() : "any : any") + " }";
            case TypeKind::FUNCTION: {
                std::string s = "fn(";
                for (size_t i = 0; i < params.size(); i++) {
                    if (i > 0) s += ", ";
                    s += params[i].toString();
                }
                s += ")";
                if (!name.empty()) s += ": " + name;
                return s;
            }
            case TypeKind::CLASS: return "class " + name;
            case TypeKind::ANY: return "any";
            default: return "unknown";
        }
    }
};

struct TypeError {
    std::string message;
    int line = 0;
    int column = 0;
    std::string severity = "error";  // "error" or "warning"
};

struct Scope {
    std::unordered_map<std::string, Type> variables;
    std::unordered_map<std::string, Type> functions;
    std::unordered_map<std::string, Type> classes;
    Scope* parent = nullptr;
    
    std::optional<Type> findVar(const std::string& name) const {
        auto it = variables.find(name);
        if (it != variables.end()) return it->second;
        if (parent) return parent->findVar(name);
        return std::nullopt;
    }
    
    std::optional<Type> findFunc(const std::string& name) const {
        auto it = functions.find(name);
        if (it != functions.end()) return it->second;
        if (parent) return parent->findFunc(name);
        return std::nullopt;
    }
    
    std::optional<Type> findClass(const std::string& name) const {
        auto it = classes.find(name);
        if (it != classes.end()) return it->second;
        if (parent) return parent->findClass(name);
        return std::nullopt;
    }
};

class TypeChecker {
public:
    TypeChecker() {
        // Initialize built-in types
        builtinTypes_["print"] = Type::function({Type::any()}, Type::nil());
        builtinTypes_["len"] = Type::function({Type::any()}, Type::integer());
        builtinTypes_["push"] = Type::function({Type::array(Type::any()), Type::any()}, Type::array(Type::any()));
        builtinTypes_["str"] = Type::function({Type::any()}, Type::string());
        builtinTypes_["int"] = Type::function({Type::any()}, Type::integer());
        builtinTypes_["float"] = Type::function({Type::any()}, Type::floating());
        builtinTypes_["type"] = Type::function({Type::any()}, Type::string());
        builtinTypes_["error"] = Type::function({Type::any()}, Type::nil());
        builtinTypes_["keys"] = Type::function({Type::map(Type::string(), Type::any())}, Type::array(Type::string()));
        builtinTypes_["values"] = Type::function({Type::map(Type::any(), Type::any())}, Type::array(Type::any()));
        builtinTypes_["has"] = Type::function({Type::map(Type::any(), Type::any()), Type::string()}, Type::boolean());
        builtinTypes_["entries"] = Type::function({Type::map(Type::any(), Type::any())}, Type::array(Type::array(Type::any())));
        builtinTypes_["clone"] = Type::function({Type::map(Type::any(), Type::any())}, Type::map(Type::any(), Type::any()));
        builtinTypes_["upper"] = Type::function({Type::string()}, Type::string());
        builtinTypes_["lower"] = Type::function({Type::string()}, Type::string());
        builtinTypes_["trim"] = Type::function({Type::string()}, Type::string());
        builtinTypes_["split"] = Type::function({Type::string(), Type::string()}, Type::array(Type::string()));
        builtinTypes_["contains"] = Type::function({Type::string(), Type::string()}, Type::boolean());
        builtinTypes_["replace"] = Type::function({Type::string(), Type::string(), Type::string()}, Type::string());
        builtinTypes_["substring"] = Type::function({Type::string(), Type::integer(), Type::integer()}, Type::string());
        builtinTypes_["charAt"] = Type::function({Type::string(), Type::integer()}, Type::string());
        builtinTypes_["parseInt"] = Type::function({Type::string()}, Type::integer());
        builtinTypes_["abs"] = Type::function({Type::integer()}, Type::integer());
        builtinTypes_["min"] = Type::function({Type::integer(), Type::integer()}, Type::integer());
        builtinTypes_["max"] = Type::function({Type::integer(), Type::integer()}, Type::integer());
        builtinTypes_["sqrt"] = Type::function({Type::floating()}, Type::floating());
        builtinTypes_["pow"] = Type::function({Type::floating(), Type::floating()}, Type::floating());
        builtinTypes_["floor"] = Type::function({Type::floating()}, Type::floating());
        builtinTypes_["ceil"] = Type::function({Type::floating()}, Type::floating());
        builtinTypes_["round"] = Type::function({Type::floating()}, Type::floating());
        builtinTypes_["random"] = Type::function({}, Type::floating());
        builtinTypes_["randomInt"] = Type::function({Type::integer(), Type::integer()}, Type::integer());
        
        // Module types
        moduleTypes_["io"] = {
            {"write", Type::function({Type::string(), Type::string()}, Type::nil())},
            {"read", Type::function({Type::string()}, Type::string())}
        };
        moduleTypes_["os"] = {
            {"time", Type::function({}, Type::integer())},
            {"execute", Type::function({Type::string()}, Type::integer())},
            {"capture", Type::function({Type::string()}, Type::string())},
            {"args", Type::function({}, Type::array(Type::string()))}
        };
        moduleTypes_["json"] = {
            {"stringify", Type::function({Type::any()}, Type::string())},
            {"parse", Type::function({Type::string()}, Type::any())}
        };
        moduleTypes_["path"] = {
            {"join", Type::function({Type::array(Type::string())}, Type::string())},
            {"base", Type::function({Type::string()}, Type::string())}
        };
        moduleTypes_["system"] = {
            {"version", Type::function({}, Type::string())},
            {"platform", Type::function({}, Type::string())}
        };
        moduleTypes_["fs"] = {
            {"read_dir", Type::function({Type::string()}, Type::array(Type::string()))},
            {"is_dir", Type::function({Type::string()}, Type::boolean())},
            {"exists", Type::function({Type::string()}, Type::boolean())},
            {"read_file", Type::function({Type::string()}, Type::string())},
            {"write_file", Type::function({Type::string(), Type::string()}, Type::boolean())},
            {"remove", Type::function({Type::string()}, Type::boolean())},
            {"rename", Type::function({Type::string(), Type::string()}, Type::boolean())},
            {"create_dir", Type::function({Type::string()}, Type::boolean())}
        };
        moduleTypes_["ffi"] = {
            {"open", Type::function({Type::string()}, Type::integer())},
            {"close", Type::function({Type::integer()}, Type::nil())},
            {"sym", Type::function({Type::integer(), Type::string()}, Type::integer())},
            {"call_int", Type::function({Type::integer(), Type::array(Type::integer())}, Type::integer())},
            {"call_float", Type::function({Type::integer(), Type::array(Type::floating())}, Type::floating())},
            {"call_str", Type::function({Type::integer(), Type::array(Type::integer())}, Type::string())},
            {"call_void", Type::function({Type::integer(), Type::array(Type::integer())}, Type::nil())},
            {"str_ptr", Type::function({Type::string()}, Type::integer())},
            {"sizeof_int", Type::function({}, Type::integer())},
            {"sizeof_long", Type::function({}, Type::integer())},
            {"sizeof_ptr", Type::function({}, Type::integer())},
            {"mem_read", Type::function({Type::integer(), Type::integer()}, Type::array(Type::integer()))},
            {"mem_write", Type::function({Type::integer(), Type::array(Type::integer())}, Type::nil())}
        };
    }
    
    std::vector<TypeError> check(const std::string& source) {
        errors_.clear();
        currentScope_ = std::make_shared<Scope>();
        
        // Parse the source
        Lexer lexer(source);
        auto tokens = lexer.scanTokens();
        Parser parser(tokens);
        auto stmts = parser.parse();
        
        if (!parser.errors().empty()) {
            for (const auto& err : parser.errors()) {
                errors_.push_back({err, 0, 0, "error"});
            }
            return errors_;
        }
        
        // Type check all statements
        for (const auto& stmt : stmts) {
            checkStatement(stmt);
        }
        
        return errors_;
    }
    
    bool hasErrors() const {
        for (const auto& e : errors_) {
            if (e.severity == "error") return true;
        }
        return false;
    }
    
private:
    std::vector<TypeError> errors_;
    std::shared_ptr<Scope> currentScope_;
    std::unordered_map<std::string, Type> builtinTypes_;
    std::unordered_map<std::string, std::unordered_map<std::string, Type>> moduleTypes_;
    std::string currentFunction_;
    
    Type checkExpression(const Expr& expr) {
        return std::visit([this](const auto& e) -> Type { return checkExpr(e); }, expr.node);
    }
    
    template<typename T>
    Type checkExpr(const T& e) {
        return Type::unknown();
    }
    
    Type checkExpr(const IntegerLiteral& e) { return Type::integer(); }
    Type checkExpr(const FloatLiteral& e) { return Type::floating(); }
    Type checkExpr(const CharLiteral& e) { return Type::string(); }
    Type checkExpr(const StringLiteral& e) { return Type::string(); }
    Type checkExpr(const BoolLiteral& e) { return Type::boolean(); }
    Type checkExpr(const NilLiteral& e) { return Type::nil(); }
    
    Type checkExpr(const Identifier& e) {
        auto var = currentScope_->findVar(e.name);
        if (var) return *var;
        auto func = currentScope_->findFunc(e.name);
        if (func) return *func;
        auto builtin = builtinTypes_.find(e.name);
        if (builtin != builtinTypes_.end()) return builtin->second;
        errors_.push_back({"Undefined variable or function: " + e.name, e.line, e.column, "error"});
        return Type::unknown();
    }
    
    Type checkExpr(const BinaryExpr& e) {
        Type left = checkExpression(*e.left);
        Type right = checkExpression(*e.right);
        
        if (e.op == "and" || e.op == "or") {
            if (!left.equals(Type::boolean()) || !right.equals(Type::boolean())) {
                errors_.push_back({"Logical operator '" + e.op + "' requires boolean operands", e.line, e.column, "error"});
            }
            return Type::boolean();
        }
        
        if (e.op == "+" || e.op == "-" || e.op == "*" || e.op == "/" || e.op == "%") {
            if (!left.isNumeric() || !right.isNumeric()) {
                errors_.push_back({"Arithmetic operator '" + e.op + "' requires numeric operands", e.line, e.column, "error"});
            }
            if (left.kind == TypeKind::FLOAT || right.kind == TypeKind::FLOAT) {
                return Type::floating();
            }
            return Type::integer();
        }
        
        if (e.op == "==" || e.op == "!=" || e.op == "<" || e.op == "<=" || e.op == ">" || e.op == ">=") {
            if (!left.equals(right)) {
                errors_.push_back({"Comparison between incompatible types: " + left.toString() + " and " + right.toString(), e.line, e.column, "warning"});
            }
            return Type::boolean();
        }
        
        if (e.op == "in") {
            if (right.kind != TypeKind::ARRAY && right.kind != TypeKind::MAP && right.kind != TypeKind::STRING) {
                errors_.push_back({"'in' operator requires array, map, or string", e.line, e.column, "error"});
            }
            return Type::boolean();
        }
        
        return Type::unknown();
    }
    
    Type checkExpr(const UnaryExpr& e) {
        Type operand = checkExpression(*e.operand);
        if (e.op == "-") {
            if (!operand.isNumeric()) {
                errors_.push_back({"Unary '-' requires numeric operand", e.line, e.column, "error"});
            }
            return operand;
        }
        if (e.op == "not") {
            if (!operand.equals(Type::boolean())) {
                errors_.push_back({"'not' requires boolean operand", e.line, e.column, "error"});
            }
            return Type::boolean();
        }
        return Type::unknown();
    }
    
    Type checkExpr(const CallExpr& e) {
        Type callee = checkExpression(*e.callee);
        std::vector<Type> argTypes;
        for (const auto& arg : e.arguments) {
            argTypes.push_back(checkExpression(*arg));
        }
        
        if (callee.kind == TypeKind::FUNCTION) {
            // Check argument count and types
            // For simplicity, we just return the return type
            if (!callee.name.empty()) {
                return Type::unknown(); // Can't determine return type without full signature
            }
            return Type::any();
        }
        
        if (callee.kind == TypeKind::CLASS) {
            return callee; // Constructor returns instance
        }
        
        return Type::any();
    }
    
    Type checkExpr(const MemberExpr& e) {
        Type object = checkExpression(*e.object);
        if (object.kind == TypeKind::CLASS) {
            // Member access on class instance
            return Type::any();
        }
        if (object.kind == TypeKind::MAP) {
            // Map access
            return object.params.size() >= 2 ? object.params[1] : Type::any();
        }
        return Type::any();
    }
    
    Type checkExpr(const IndexExpr& e) {
        Type object = checkExpression(*e.object);
        Type index = checkExpression(*e.index);
        
        if (object.kind == TypeKind::ARRAY) {
            if (!index.equals(Type::integer())) {
                errors_.push_back({"Array index must be integer", e.line, e.column, "error"});
            }
            return object.params.empty() ? Type::any() : object.params[0];
        }
        if (object.kind == TypeKind::MAP) {
            return object.params.size() >= 2 ? object.params[1] : Type::any();
        }
        if (object.kind == TypeKind::STRING) {
            if (!index.equals(Type::integer())) {
                errors_.push_back({"String index must be integer", e.line, e.column, "error"});
            }
            return Type::string();
        }
        return Type::any();
    }
    
    Type checkExpr(const ArrayLiteral& e) {
        Type elemType = Type::unknown();
        for (const auto& elem : e.elements) {
            Type t = checkExpression(*elem);
            if (elemType.kind == TypeKind::UNKNOWN) elemType = t;
            else if (!t.equals(elemType)) {
                errors_.push_back({"Array element type mismatch: " + elemType.toString() + " vs " + t.toString(), e.line, e.column, "warning"});
            }
        }
        if (elemType.kind == TypeKind::UNKNOWN) elemType = Type::any();
        return Type::array(elemType);
    }
    
    Type checkExpr(const MapLiteral& e) {
        Type keyType = Type::unknown();
        Type valType = Type::unknown();
        for (const auto& [k, v] : e.entries) {
            Type kt = checkExpression(*k);
            Type vt = checkExpression(*v);
            if (keyType.kind == TypeKind::UNKNOWN) keyType = kt;
            else if (!kt.equals(keyType)) {
                errors_.push_back({"Map key type mismatch", e.line, e.column, "warning"});
            }
            if (valType.kind == TypeKind::UNKNOWN) valType = vt;
            else if (!vt.equals(valType)) {
                errors_.push_back({"Map value type mismatch", e.line, e.column, "warning"});
            }
        }
        if (keyType.kind == TypeKind::UNKNOWN) keyType = Type::any();
        if (valType.kind == TypeKind::UNKNOWN) valType = Type::any();
        return Type::map(keyType, valType);
    }
    
    Type checkExpr(const FnExpr& e) {
        // Lambda type checking
        std::vector<Type> paramTypes;
        for (const auto& param : e.params) {
            paramTypes.push_back(param.typeHint ? parseTypeAnnotation(*param.typeHint) : Type::any());
        }
        
        auto oldScope = currentScope_;
        currentScope_ = std::make_shared<Scope>();
        currentScope_->parent = oldScope.get();
        
        for (size_t i = 0; i < e.params.size(); i++) {
            currentScope_->variables[e.params[i].name] = paramTypes[i];
        }
        
        Type returnType = Type::nil();
        for (const auto& stmt : e.body) {
            if (stmt.type() == StmtType::RETURN) {
                // Find return value
                auto ret = std::get<ReturnStmt>(stmt.node);
                if (ret.value) returnType = checkExpression(*ret.value);
                else returnType = Type::nil();
                break;
            }
            checkStatement(stmt);
        }
        
        currentScope_ = oldScope;
        return Type::function(paramTypes, returnType);
    }
    
    Type checkExpr(const SuperExpr& e) { return Type::any(); }
    Type checkExpr(const StringInterpExpr& e) { return Type::string(); }
    Type checkExpr(const MatchExpr& e) { 
        Type subject = checkExpression(*e.subject);
        Type resultType = Type::unknown();
        for (const auto& arm : e.arms) {
            Type armType = checkExpression(*arm.body);
            if (resultType.kind == TypeKind::UNKNOWN) resultType = armType;
            else if (!armType.equals(resultType)) {
                errors_.push_back({"Match arms have different types: " + resultType.toString() + " vs " + armType.toString(), e.line, e.column, "warning"});
            }
        }
        return resultType.kind == TypeKind::UNKNOWN ? Type::any() : resultType;
    }
    Type checkExpr(const ThrowExpr& e) { return checkExpression(*e.value); }
    Type checkExpr(const YieldExpr& e) { return e.value ? checkExpression(*e.value) : Type::nil(); }
    Type checkExpr(const AssignExpr& e) {
        Type value = checkExpression(*e.value);
        // Check if variable exists and type matches
        auto var = currentScope_->findVar(e.name);
        if (var && !var->equals(Type::any()) && !var->equals(value)) {
            errors_.push_back({"Assignment type mismatch: " + var->toString() + " = " + value.toString(), e.line, e.column, "error"});
        }
        return value;
    }
    Type checkExpr(const AssignMemberExpr& e) {
        checkExpression(*e.object);
        return checkExpression(*e.value);
    }
    Type checkExpr(const AssignIndexExpr& e) {
        checkExpression(*e.object);
        checkExpression(*e.index);
        return checkExpression(*e.value);
    }
    Type checkExpr(const ThisExpr& e) { return Type::any(); }
    
    void checkStatement(const Stmt& stmt) {
        std::visit([this](const auto& s) { checkStmt(s); }, stmt.node);
    }
    
    template<typename T>
    void checkStmt(const T& s) {}
    
    void checkStmt(const ExprStmt& s) { checkExpression(*s.expression); }
    
    void checkStmt(const VarDecl& s) {
        Type initType = checkExpression(*s.initializer);
        Type declaredType = s.typeHint ? parseTypeAnnotation(*s.typeHint) : initType;
        
        if (!initType.equals(declaredType) && declaredType.kind != TypeKind::ANY && initType.kind != TypeKind::UNKNOWN) {
            errors_.push_back({"Variable type mismatch: declared " + declaredType.toString() + " but got " + initType.toString(), s.line, s.column, "error"});
        }
        currentScope_->variables[s.name] = declaredType;
    }
    
    void checkStmt(const ReturnStmt& s) {
        if (s.value) checkExpression(*s.value);
    }
    
    void checkStmt(const IfStmt& s) {
        Type cond = checkExpression(*s.condition);
        if (!cond.equals(Type::boolean()) && cond.kind != TypeKind::ANY) {
            errors_.push_back({"If condition must be boolean", s.line, s.column, "error"});
        }
        auto oldScope = currentScope_;
        currentScope_ = std::make_shared<Scope>();
        currentScope_->parent = oldScope.get();
        for (const auto& stmt : s.thenBranch) checkStatement(stmt);
        currentScope_ = oldScope;
        if (s.elseBranch) {
            currentScope_ = std::make_shared<Scope>();
            currentScope_->parent = oldScope.get();
            for (const auto& stmt : s.elseBranch) checkStatement(stmt);
            currentScope_ = oldScope;
        }
    }
    
    void checkStmt(const WhileStmt& s) {
        Type cond = checkExpression(*s.condition);
        if (!cond.equals(Type::boolean()) && cond.kind != TypeKind::ANY) {
            errors_.push_back({"While condition must be boolean", s.line, s.column, "error"});
        }
        auto oldScope = currentScope_;
        currentScope_ = std::make_shared<Scope>();
        currentScope_->parent = oldScope.get();
        for (const auto& stmt : s.body) checkStatement(stmt);
        currentScope_ = oldScope;
    }
    
    void checkStmt(const ForStmt& s) {
        auto oldScope = currentScope_;
        currentScope_ = std::make_shared<Scope>();
        currentScope_->parent = oldScope.get();
        if (s.initializer) checkStatement(*s.initializer);
        if (s.condition) {
            Type cond = checkExpression(*s.condition);
            if (!cond.equals(Type::boolean()) && cond.kind != TypeKind::ANY) {
                errors_.push_back({"For condition must be boolean", s.line, s.column, "error"});
            }
        }
        if (s.increment) checkExpression(*s.increment);
        for (const auto& stmt : s.body) checkStatement(stmt);
        currentScope_ = oldScope;
    }
    
    void checkStmt(const ForInStmt& s) {
        Type iterable = checkExpression(*s.iterable);
        if (iterable.kind != TypeKind::ARRAY && iterable.kind != TypeKind::MAP && iterable.kind != TypeKind::STRING && iterable.kind != TypeKind::ANY) {
            errors_.push_back({"for...in requires iterable (array, map, or string)", s.line, s.column, "error"});
        }
        Type elemType = iterable.kind == TypeKind::ARRAY ? (iterable.params.empty() ? Type::any() : iterable.params[0]) : Type::string();
        auto oldScope = currentScope_;
        currentScope_ = std::make_shared<Scope>();
        currentScope_->parent = oldScope.get();
        currentScope_->variables[s.variable] = elemType;
        for (const auto& stmt : s.body) checkStatement(stmt);
        currentScope_ = oldScope;
    }
    
    void checkStmt(const BlockStmt& s) {
        auto oldScope = currentScope_;
        currentScope_ = std::make_shared<Scope>();
        currentScope_->parent = oldScope.get();
        for (const auto& stmt : s.statements) checkStatement(stmt);
        currentScope_ = oldScope;
    }
    
    void checkStmt(const FnDecl& s) {
        std::vector<Type> paramTypes;
        for (const auto& param : s.params) {
            paramTypes.push_back(param.typeHint ? parseTypeAnnotation(*param.typeHint) : Type::any());
        }
        Type returnType = s.returnType ? parseTypeAnnotation(*s.returnType) : Type::any();
        
        Type fnType = Type::function(paramTypes, returnType);
        currentScope_->functions[s.name] = fnType;
        
        auto oldScope = currentScope_;
        currentScope_ = std::make_shared<Scope>();
        currentScope_->parent = oldScope.get();
        currentFunction_ = s.name;
        
        for (size_t i = 0; i < s.params.size(); i++) {
            currentScope_->variables[s.params[i].name] = paramTypes[i];
        }
        
        for (const auto& stmt : s.body) checkStatement(stmt);
        
        currentScope_ = oldScope;
        currentFunction_ = "";
    }
    
    void checkStmt(const GenDecl& s) { checkStmt(static_cast<const FnDecl&>(s)); }
    
    void checkStmt(const ClassDecl& s) {
        Type classType = Type::classType(s.name);
        currentScope_->classes[s.name] = classType;
        
        auto oldScope = currentScope_;
        currentScope_ = std::make_shared<Scope>();
        currentScope_->parent = oldScope.get();
        
        // Add 'this' type
        currentScope_->variables["this"] = classType;
        
        for (const auto& method : s.methods) {
            if (method.type() == StmtType::FN_DECL) {
                auto fn = std::get<FnDecl>(method.node);
                std::vector<Type> paramTypes;
                for (const auto& param : fn.params) {
                    paramTypes.push_back(param.typeHint ? parseTypeAnnotation(*param.typeHint) : Type::any());
                }
                Type returnType = fn.returnType ? parseTypeAnnotation(*fn.returnType) : Type::any();
                currentScope_->functions[fn.name] = Type::function(paramTypes, returnType);
            }
        }
        
        for (const auto& method : s.methods) checkStatement(method);
        
        currentScope_ = oldScope;
    }
    
    void checkStmt(const StructDecl& s) {
        Type structType = Type::classType(s.name);
        currentScope_->classes[s.name] = structType;
    }
    
    void checkStmt(const ImplDecl& s) {
        // Extension methods
    }
    
    void checkStmt(const EnumDecl& s) {
        Type enumType = Type::classType(s.name);
        currentScope_->classes[s.name] = enumType;
    }
    
    void checkStmt(const TryStmt& s) {
        auto oldScope = currentScope_;
        currentScope_ = std::make_shared<Scope>();
        currentScope_->parent = oldScope.get();
        for (const auto& stmt : s.tryBlock) checkStatement(stmt);
        currentScope_ = oldScope;
        
        if (s.catchVar) {
            currentScope_ = std::make_shared<Scope>();
            currentScope_->parent = oldScope.get();
            currentScope_->variables[*s.catchVar] = Type::any();
            for (const auto& stmt : s.catchBlock) checkStatement(stmt);
            currentScope_ = oldScope;
        }
        
        if (s.finallyBlock) {
            currentScope_ = std::make_shared<Scope>();
            currentScope_->parent = oldScope.get();
            for (const auto& stmt : s.finallyBlock) checkStatement(stmt);
            currentScope_ = oldScope;
        }
    }
    
    void checkStmt(const ExternFnDecl& s) {
        std::vector<Type> paramTypes;
        for (const auto& param : s.params) {
            paramTypes.push_back(param.typeHint ? parseTypeAnnotation(*param.typeHint) : Type::any());
        }
        Type returnType = s.returnType ? parseTypeAnnotation(*s.returnType) : Type::any();
        currentScope_->functions[s.name] = Type::function(paramTypes, returnType);
    }
    
    void checkStmt(const ImportStmt& s) {
        // Check if module exists
        if (moduleTypes_.find(s.module) == moduleTypes_.end()) {
            errors_.push_back({"Unknown module: " + s.module, s.line, s.column, "warning"});
        }
        // Add module namespace
        if (s.alias) {
            // Aliased import - would add to scope
        }
    }
    
    void checkStmt(const BreakStmt& s) {}
    void checkStmt(const ContinueStmt& s) {}
    
    Type parseTypeAnnotation(const TypeAnnotation& ann) {
        if (ann.name == "nil") return Type::nil();
        if (ann.name == "bool") return Type::boolean();
        if (ann.name == "int") return Type::integer();
        if (ann.name == "float") return Type::floating();
        if (ann.name == "string") return Type::string();
        if (ann.name == "any") return Type::any();
        if (ann.name.rfind("[]", 0) == 0) {
            // Array type
            return Type::array(parseTypeAnnotation(*ann.params[0]));
        }
        if (ann.name.rfind("{}", 0) == 0) {
            // Map type
            return Type::map(parseTypeAnnotation(*ann.params[0]), parseTypeAnnotation(*ann.params[1]));
        }
        if (ann.name.rfind("fn", 0) == 0) {
            // Function type
            std::vector<Type> params;
            for (const auto& p : ann.params) {
                params.push_back(parseTypeAnnotation(*p));
            }
            return Type::function(params, Type::any());
        }
        // Class type
        return Type::classType(ann.name);
    }
};

} // namespace forge::typecheck