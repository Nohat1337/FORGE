#include "java_backend.hpp"
#include "android_templates.h"
#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <cctype>
#include <functional>

namespace fs = std::filesystem;

namespace forge::javabackend {

namespace {

// ------------------------------------------------------------
// Name sanitization / mangling
// ------------------------------------------------------------

std::string sanitizeIdent(const std::string& n) {
    std::string out;
    if (n.empty()) return "_";
    if (std::isdigit((unsigned char)n[0])) out += "_";
    for (char c : n) {
        if (std::isalnum((unsigned char)c) || c == '_') out += c;
        else out += "_";
    }
    // avoid Java keywords
    static const char* kws[] = {"abstract","assert","boolean","break","byte","case","catch","char",
        "class","const","continue","default","do","double","else","enum","extends","final","finally",
        "float","for","goto","if","implements","import","instanceof","int","interface","long","native",
        "new","package","private","protected","public","return","short","static","strictfp","super",
        "switch","synchronized","this","throw","throws","transient","try","void","volatile","while",
        "true","false","null","var","record","yield"};
    for (auto* k : kws) if (out == k) return out + "_";
    return out;
}

// Quote an argument for /bin/sh — std::system always goes through the shell,
// so user-controlled paths must not be interpolated raw (command injection).
std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}

std::string escapeJava(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 32 || c > 126) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else out += (char)c;
        }
    }
    return out;
}

// ------------------------------------------------------------
// Code generator
// ------------------------------------------------------------

class Gen {
public:
    explicit Gen(const std::vector<StmtPtr>& program, const std::string& mainName)
        : program_(program), mainName_(sanitizeIdent(mainName)) {}

    struct CompileError : std::runtime_error {
        CompileError(const std::string& m, int line)
            : std::runtime_error("line " + std::to_string(line) + ": " + m) {}
    };

    std::string generate() {
        scanTopLevel();
        emitPrelude();
        emitMembers();       // globals + methods + classes as Java members
        emitMain();          // top-level statements in main()
        closeClass();
        return out_.str();
    }

private:
    const std::vector<StmtPtr>& program_;
    std::string mainName_;
    std::ostringstream out_;
    int tmp_ = 0;

    std::unordered_set<std::string> globalFns_;   // top-level fn names -> static methods
    std::unordered_set<std::string> globalVars_;  // top-level var names
    std::unordered_set<std::string> classNames_;
    std::vector<std::string> memberDecls_;   // emitted before main body is known
    std::ostringstream members_;

    // scope stack of local names (Cell-wrapped in generated code)
    std::vector<std::unordered_set<std::string>> scopes_;
    bool scriptTopLevel_ = false;   // generating main()'s top-level statements
    std::vector<std::pair<std::string, std::string>> imports_; // alias, module
    std::vector<std::pair<std::string, const ClassDecl*>> classRegs_;

    int nextTmp() { return ++tmp_; }
    std::string tmp(const std::string& hint) { return "__" + hint + std::to_string(nextTmp()); }

    bool isLocal(const std::string& n) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
            if (it->count(n)) return true;
        return false;
    }

    void scanTopLevel() {
        for (auto& s : program_) {
            if (auto* f = std::get_if<FnDecl>(&s->node)) globalFns_.insert(f->name);
            else if (auto* c = std::get_if<ClassDecl>(&s->node)) {
                classNames_.insert(c->name);
                classRegs_.push_back({c->name, c});
            } else if (auto* v = std::get_if<VarDecl>(&s->node)) globalVars_.insert(v->name);
            else if (auto* i = std::get_if<ImportStmt>(&s->node))
                imports_.push_back({i->alias.empty() ? "" : i->alias, i->moduleName});
        }
    }

    // ---------------- runtime prelude ----------------

    void emitPrelude() {
        out_ << "// Generated by Forge -> Java backend (forgevm java)\n"
             << "// Source language: Forge (https://github.com/Nohat1337/FORGE)\n"
             << "import java.util.*;\n"
             << "import java.util.regex.Pattern;\n"
             << "import java.nio.file.*;\n"
             << "import java.util.concurrent.ThreadLocalRandom;\n\n"
             << "public class " << mainName_ << " {\n\n"
             << RUNTIME_SRC;
    }

    // Embedded runtime shipped inside every generated jar.
    static constexpr const char* RUNTIME_SRC =
R"JAVA(    // ================= Forge Runtime (R) =================
    @SuppressWarnings("unchecked")
    public static class R {
        public static class ForgeError extends RuntimeException {
            public ForgeError(String m) { super(m); }
        }
        // mutable cell so closures can capture variables correctly
        public static class Cell { public Object v;
            public Cell(Object v0) { v = v0; } }
        // function value
        public interface Fn { Object call(List<Object> args); }
        // class instance
        public static class Obj {
            public final String klass;
            public final Map<String,Object> fields = new LinkedHashMap<>();
            public Obj(String k) { klass = k; }
            public String toString() { return "<" + klass + ">"; }
        }
        public static List<Object> ARGS = new ArrayList<>();
        public static final Map<String,Fn> CLASSES = new HashMap<>();
        public static final Map<String,String> SUPER = new HashMap<>();

        public static boolean truth(Object o) {
            if (o == null) return false;
            if (o instanceof Boolean) return (Boolean)o;
            return true;
        }
        public static boolean isNum(Object o) { return o instanceof Long || o instanceof Double; }
        public static double num(Object o) {
            if (o instanceof Long) return (Long)o;
            if (o instanceof Double) return (Double)o;
            throw new ForgeError("expected number, got " + typeOf(o));
        }
        public static long integer(Object o) {
            if (o instanceof Long) return (Long)o;
            if (o instanceof Double) return Math.round((Double)o);
            throw new ForgeError("expected integer, got " + typeOf(o));
        }
        public static String str(Object o) {
            if (o == null) return "nil";
            if (o instanceof Boolean) return (Boolean)o ? "true" : "false";
            if (o instanceof Double) {
                double d = (Double)o;
                if (d == Math.rint(d) && !Double.isInfinite(d) && Math.abs(d) < 1e15)
                    return String.format(Locale.ROOT, "%.1f", d);
                return String.valueOf(d);
            }
            return String.valueOf(o);
        }
        public static String typeOf(Object o) {
            if (o == null) return "nil";
            if (o instanceof Boolean) return "bool";
            if (o instanceof Long) return "int";
            if (o instanceof Double) return "float";
            if (o instanceof String) return "string";
            if (o instanceof ArrayList) return "array";
            if (o instanceof LinkedHashMap) return "map";
            if (o instanceof Obj) return "instance";
            if (o instanceof Fn) return "function";
            return "object";
        }
        public static boolean eq(Object a, Object b) {
            if (a == null && b == null) return true;
            if (isNum(a) && isNum(b)) return num(a) == num(b);
            return Objects.equals(a, b);
        }
        public static boolean neq(Object a, Object b) { return !eq(a, b); }
        private static int cmp(Object a, Object b, String op) {
            if (a instanceof String && b instanceof String)
                return ((String)a).compareTo((String)b);
            double x = num(a), y = num(b);
            return x < y ? -1 : x > y ? 1 : 0;
        }
        public static boolean lt(Object a, Object b) { return cmp(a,b,"<") < 0; }
        public static boolean le(Object a, Object b) { return cmp(a,b,"<=") <= 0; }
        public static boolean gt(Object a, Object b) { return cmp(a,b,">") > 0; }
        public static boolean ge(Object a, Object b) { return cmp(a,b,">=") >= 0; }
        public static Object add(Object a, Object b) {
            if (a instanceof String || b instanceof String) return str(a) + str(b);
            if (a instanceof Long && b instanceof Long) {
                long r = (Long)a + (Long)b;   // JVM wraps on overflow; documented
                return r;
            }
            return num(a) + num(b);
        }
        public static Object sub(Object a, Object b) {
            if (a instanceof Long && b instanceof Long) return (Long)a - (Long)b;
            return num(a) - num(b);
        }
        public static Object mul(Object a, Object b) {
            if (a instanceof Long && b instanceof Long) return (Long)a * (Long)b;
            return num(a) * num(b);
        }
        public static Object div(Object a, Object b) {
            if (num(b) == 0) throw new ForgeError("Division by zero");
            if (a instanceof Long && b instanceof Long) return (Long)a / (Long)b;
            return num(a) / num(b);
        }
        public static Object mod(Object a, Object b) {
            if (num(b) == 0) throw new ForgeError("Division by zero");
            if (a instanceof Long && b instanceof Long) return (Long)a % (Long)b;
            return num(a) % num(b);
        }
        public static Object neg(Object o) {
            if (o instanceof Long) return -(Long)o;
            return -num(o);
        }
        public static boolean not(Object o) { return !truth(o); }
        public static Object band(Object a, Object b) { return integer(a) & integer(b); }
        public static Object bor(Object a, Object b) { return integer(a) | integer(b); }
        public static Object bxor(Object a, Object b) { return integer(a) ^ integer(b); }

        public static ArrayList<Object> arr(Object... items) {
            ArrayList<Object> l = new ArrayList<>();
            Collections.addAll(l, items);
            return l;
        }
        public static LinkedHashMap<String,Object> map(Object... kv) {
            LinkedHashMap<String,Object> m = new LinkedHashMap<>();
            for (int i = 0; i + 1 < kv.length; i += 2)
                m.put(str(kv[i]), kv[i+1]);
            return m;
        }
        public static Object idx(Object o, Object i) {
            if (o instanceof ArrayList) {
                ArrayList<Object> a = (ArrayList<Object>)o;
                int k = (int)integer(i);
                if (k < 0 || k >= a.size())
                    throw new ForgeError("Array index " + k + " out of bounds (length " + a.size() + ")");
                return a.get(k);
            }
            if (o instanceof String) {
                String s = (String)o;
                int k = (int)integer(i);
                if (k < 0 || k >= s.length())
                    throw new ForgeError("String index " + k + " out of bounds");
                return String.valueOf(s.charAt(k));
            }
            if (o instanceof Map) return ((Map<String,Object>)o).get(str(i));
            throw new ForgeError("Cannot index into " + typeOf(o));
        }
        public static Object setIdx(Object o, Object i, Object v) {
            if (o instanceof ArrayList) {
                ArrayList<Object> a = (ArrayList<Object>)o;
                int k = (int)integer(i);
                if (k < 0 || k >= a.size())
                    throw new ForgeError("Array index " + k + " out of bounds");
                return a.set(k, v);
            }
            if (o instanceof Map) return ((Map<String,Object>)o).put(str(i), v);
            throw new ForgeError("Cannot set index on " + typeOf(o));
        }
        public static Object get(Object o, String name) {
            if (o instanceof Obj) {
                Obj inst = (Obj)o;
                if (inst.fields.containsKey(name)) return inst.fields.get(name);
                Fn f = lookupMethod(inst.klass, name);
                if (f != null) {
                    final Obj recv = inst;
                    return (Fn) (args -> { List<Object> a2 = new ArrayList<>();
                        a2.add(recv); a2.addAll(args); return f.call(a2); });
                }
                throw new ForgeError("Undefined property '" + name + "' on " + inst.klass);
            }
            if (o instanceof Map) {
                Map<String,Object> m = (Map<String,Object>)o;
                if (!m.containsKey(name)) throw new ForgeError("Undefined property '" + name + "'");
                return m.get(name);
            }
            throw new ForgeError("Cannot access property '" + name + "' on " + typeOf(o));
        }
        public static Object set(Object o, String name, Object v) {
            if (o instanceof Obj) { ((Obj)o).fields.put(name, v); return v; }
            if (o instanceof Map) { ((Map<String,Object>)o).put(name, v); return v; }
            throw new ForgeError("Cannot set property '" + name + "' on " + typeOf(o));
        }
        static Fn lookupMethod(String klass, String name) {
            String k = klass;
            while (k != null) {
                Fn f = CLASSES.get(k + "." + name);
                if (f != null) return f;
                k = SUPER.get(k);
            }
            return null;
        }
        public static Object call(Object fn, List<Object> args) {
            if (!(fn instanceof Fn)) throw new ForgeError("Cannot call " + typeOf(fn));
            return ((Fn)fn).call(args);
        }
        public static Object mcall(Object o, String name, List<Object> args) {
            if (o instanceof String) {
                String s = (String)o;
                switch (name) {
                    case "upper": return s.toUpperCase();
                    case "lower": return s.toLowerCase();
                    case "trim": return s.trim();
                    case "split": return arr((Object)s.split(Pattern.quote(str(args.get(0)))));
                    case "contains": return s.contains(str(args.get(0)));
                    case "replace": return s.replace(str(args.get(0)), str(args.get(1)));
                    case "substring": {
                        int a = (int)integer(args.get(0));
                        int b = args.size() > 1 ? (int)integer(args.get(1)) : s.length();
                        b = Math.min(Math.max(b, a), s.length());
                        return s.substring(Math.max(0,a), b);
                    }
                    case "charAt": return String.valueOf(s.charAt((int)integer(args.get(0))));
                    case "parseInt": try { return Long.parseLong(s.trim()); }
                        catch (NumberFormatException e) { throw new ForgeError("parseInt: invalid integer \"" + s + "\""); }
                    case "toFloat": try { return Double.parseDouble(s.trim()); }
                        catch (NumberFormatException e) { throw new ForgeError("toFloat: invalid number \"" + s + "\""); }
                }
            }
            if (o instanceof ArrayList) {
                ArrayList<Object> a = (ArrayList<Object>)o;
                if (name.equals("push")) { a.add(args.get(0)); return a; }
                if (name.equals("pop")) return a.remove(a.size() - 1);
                if (name.equals("contains")) return a.contains(args.get(0));
                if (name.equals("clone")) return (ArrayList<Object>)a.clone();
                if (name.equals("indexOf")) return (long)a.indexOf(args.get(0));
            }
            if (o instanceof Map) {
                Map<String,Object> m = (Map<String,Object>)o;
                switch (name) {
                    case "keys": return arr(m.keySet().toArray());
                    case "values": return arr(m.values().toArray());
                    case "has": return m.containsKey(str(args.get(0)));
                    case "clone": return (Map<String,Object>)((LinkedHashMap<String,Object>)o).clone();
                    case "remove": return m.remove(str(args.get(0)));
                }
            }
            if (o instanceof Obj) {
                Fn f = lookupMethod(((Obj)o).klass, name);
                if (f != null) {
                    List<Object> a2 = new ArrayList<>();
                    a2.add(o);
                    a2.addAll(args);
                    return f.call(a2);
                }
                throw new ForgeError("Undefined method '" + name + "' on " + ((Obj)o).klass);
            }
            throw new ForgeError("No method '" + name + "' on " + typeOf(o));
        }
        public static Iterable<Object> iter(Object o) {
            if (o instanceof ArrayList) return (ArrayList<Object>)o;
            if (o instanceof Map) return new ArrayList<Object>(((Map<String,Object>)o).keySet());
            if (o instanceof String) {
                ArrayList<Object> cs = new ArrayList<>();
                for (char c : ((String)o).toCharArray()) cs.add(String.valueOf(c));
                return cs;
            }
            throw new ForgeError("Cannot iterate over " + typeOf(o));
        }
        public static Object newInst(String klass, List<Object> args) {
            Obj o = new Obj(klass);
            Fn init = lookupMethod(klass, "init");
            if (init != null) {
                List<Object> a2 = new ArrayList<>();
                a2.add(o);
                a2.addAll(args);
                init.call(a2);
            } else if (!args.isEmpty()) {
                throw new ForgeError("Expected 0 arguments for constructor of " + klass);
            }
            return o;
        }
        public static Object superCall(Object self, String name, List<Object> args) {
            if (!(self instanceof Obj)) throw new ForgeError("super outside of instance method");
            String sup = SUPER.get(((Obj)self).klass);
            Fn f = sup == null ? null : CLASSES.get(sup + "." + name);
            if (f == null) throw new ForgeError("Undefined super method '" + name + "'");
            List<Object> a2 = new ArrayList<>();
            a2.add(self);
            a2.addAll(args);
            return f.call(a2);
        }

        // ---- builtins ----
        public static Object print(Object... args) {
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < args.length; i++) {
                if (i > 0) sb.append(' ');
                sb.append(str(args[i]));
            }
            System.out.println(sb);
            return null;
        }
        public static Object len(Object... a) {
            Object o = a[0];
            if (o instanceof String) return (long)((String)o).length();
            if (o instanceof ArrayList) return (long)((ArrayList<?>)o).size();
            if (o instanceof Map) return (long)((Map<?,?>)o).size();
            throw new ForgeError("len() expects string, array or map");
        }
        public static Object push(Object... a) {
            ((ArrayList<Object>)a[0]).add(a[1]); return a[0];
        }
        public static Object keys(Object... a) {
            return arr(((Map<String,Object>)a[0]).keySet().toArray()); }
        public static Object values(Object... a) {
            return arr(((Map<String,Object>)a[0]).values().toArray()); }
        public static Object has(Object... a) {
            return ((Map<String,Object>)a[0]).containsKey(str(a[1])); }
        public static Object entries(Object... a) {
            ArrayList<Object> out = new ArrayList<>();
            for (Map.Entry<String,Object> e : ((Map<String,Object>)a[0]).entrySet())
                out.add(arr(e.getKey(), e.getValue()));
            return out;
        }
        public static Object cloneV(Object... a) {
            Object o = a[0];
            if (o instanceof ArrayList) return ((ArrayList<Object>)o).clone();
            if (o instanceof LinkedHashMap) return ((LinkedHashMap<String,Object>)o).clone();
            throw new ForgeError("clone() expects array or map");
        }
        public static Object errorFn(Object... a) {
            throw new ForgeError(str(a[0])); }
        public static Object toInt(Object... a) { return integer(a[0]); }
        public static Object toFloatV(Object... a) { return num(a[0]); }
        public static Object abs(Object... a) {
            Object o = a[0];
            if (o instanceof Long) return Math.abs((Long)o);
            return Math.abs(num(o));
        }
        public static Object min(Object... a) { return cmp(a[0],a[1],"min") < 0 ? a[0] : a[1]; }
        public static Object max(Object... a) { return cmp(a[0],a[1],"max") > 0 ? a[0] : a[1]; }
        public static Object sqrt(Object... a) { return Math.sqrt(num(a[0])); }
        public static Object pow(Object... a) { return Math.pow(num(a[0]), num(a[1])); }
        public static Object floor(Object... a) { return (long)Math.floor(num(a[0])); }
        public static Object ceil(Object... a) { return (long)Math.ceil(num(a[0])); }
        public static Object round(Object... a) { return Math.round(num(a[0])); }
        public static Object random() { return ThreadLocalRandom.current().nextDouble(); }
        public static Object randomInt(Object... a) {
            int lo = (int)integer(a[0]), hi = (int)integer(a[1]);
            if (lo > hi) throw new ForgeError("randomInt() min must be <= max");
            return (long)ThreadLocalRandom.current().nextInt(lo, hi + 1);
        }
        // io/os modules
        public static Object ioRead(List<Object> a) {
            try { return new String(Files.readAllBytes(Paths.get(str(a.get(0))))); }
            catch (Exception e) { throw new ForgeError("io.read() cannot open file: " + str(a.get(0))); }
        }
        public static Object ioWrite(List<Object> a) {
            try {
                Files.write(Paths.get(str(a.get(0))), str(a.get(1)).getBytes());
                return null;
            } catch (Exception e) { throw new ForgeError("io.write() failed: " + e.getMessage()); }
        }
        public static Object ioExists(List<Object> a) {
            return Files.exists(Paths.get(str(a.get(0)))); }
        public static Object osTime(List<Object> a) {
            return System.currentTimeMillis(); }
        public static Object osEnv(List<Object> a) {
            return System.getenv(str(a.get(0))); }
        public static Object systemVersion(List<Object> a) { return "2.0.0-java"; }
    }
    // =============== end Forge Runtime (R) ===============
)JAVA";


    void closeClass() { out_ << "}\n"; }

    // ---------------- members (globals, fns, classes) ----------------

    // true if a generated body already ends with return/throw on all paths
    static bool bodyNeverFallsThrough(const std::vector<StmtPtr>& b) {
        if (b.empty()) return false;
        if (std::get_if<ReturnStmt>(&b.back()->node)) return true;
        if (auto* e = std::get_if<ExprStmt>(&b.back()->node))
            return std::get_if<ThrowExpr>(&e->expression->node) != nullptr;
        return false;
    }

    void emitMembers() {
        // top-level variable declarations as static fields
        for (auto& s : program_) {
            if (auto* v = std::get_if<VarDecl>(&s->node)) {
                members_ << "    static Object " << sanitizeIdent(v->name)
                         << " = null; // initialized in main\n";
            }
        }
        // top-level functions
        for (auto& s : program_)
            if (auto* f = std::get_if<FnDecl>(&s->node)) emitFnDecl(*f);

        // classes
        for (auto& [cname, cd] : classRegs_) emitClassMethods(cname, *cd);

        // static arity table + accessor
        members_ << "    private static final Map<String,Integer> ARITY = new HashMap<>();\n"
                 << "    private static int arityOf(String key) { Integer v = ARITY.get(key); return v == null ? 1 : v; }\n"
                 << "    static {\n";
        for (auto& [cname, cd] : classRegs_) {
            for (auto& m : cd->methods)
                members_ << "        ARITY.put(\"" << escapeJava(cname) << "."
                         << escapeJava(m.name) << "\", " << (m.params.size() + 1) << ");\n";
        }
        members_ << "    }\n";

        // method registration executed at startup
        members_ << "    @SuppressWarnings(\"unchecked\")\n"
                 << "    private static void registerClasses() {\n";
        for (auto& [cname, cd] : classRegs_) {
            std::string superName;
            if (cd->superclass) {
                if (auto* id = std::get_if<Identifier>(&cd->superclass->node))
                    superName = id->name;
            }
            if (!superName.empty())
                members_ << "        R.SUPER.put(\"" << escapeJava(cname) << "\", \""
                         << escapeJava(superName) << "\");\n";
            for (auto& m : cd->methods)
                members_ << "        R.CLASSES.put(\"" << escapeJava(cname) << "."
                         << escapeJava(m.name) << "\", "
                         << lambdaForMethod(cname, m) << ");\n";
        }
        members_ << "    }\n\n";
    }

    void emitFnDecl(const FnDecl& f) {
        std::ostringstream sig;
        sig << "    private static Object " << sanitizeIdent(f.name) << "(";
        for (size_t i = 0; i < f.params.size(); i++) {
            if (i) sig << ", ";
            sig << "Object __p" << i;
        }
        sig << ") {\n";
        members_ << sig.str();

        scopes_.push_back({});
        auto& sc = scopes_.back();
        for (size_t i = 0; i < f.params.size(); i++) {
            sc.insert(f.params[i]);
            members_ << "        R.Cell " << sanitizeIdent(f.params[i])
                     << " = new R.Cell(__p" << i << ");\n";
        }
        genBodyInto(members_, f.body, true);
        scopes_.pop_back();
        if (!lastBlockValue_.empty())
            members_ << "        return " << lastBlockValue_ << ";\n";
        else if (!bodyNeverFallsThrough(f.body))
            members_ << "        return null;\n";
        members_ << "    }\n\n";
    }

    void emitClassMethods(const std::string& cname, const ClassDecl& cd) {
        for (auto& m : cd.methods) {
            members_ << "    private static Object " << sanitizeIdent(cname) << "_"
                     << sanitizeIdent(m.name) << "(R.Obj self, Object... __rest) {\n";
            scopes_.push_back({});
            auto& sc = scopes_.back();
            sc.insert("this");
            members_ << "        R.Cell th = new R.Cell(self);\n";
            for (size_t i = 0; i < m.params.size(); i++) {
                sc.insert(m.params[i]);
                members_ << "        R.Cell " << sanitizeIdent(m.params[i])
                         << " = new R.Cell(__rest.length > " << i << " ? __rest[" << i
                         << "] : null);\n";
            }
            genBodyInto(members_, m.body, true);
            scopes_.pop_back();
            if (!lastBlockValue_.empty())
                members_ << "        return " << lastBlockValue_ << ";\n";
            else if (!bodyNeverFallsThrough(m.body))
                members_ << "        return null;\n";
            members_ << "    }\n\n";
        }
    }

    // ---------------- statements ----------------

    void indent(std::ostringstream& o, int d) { for (int i = 0; i < d; i++) o << "    "; }

    void genBodyInto(std::ostringstream& o, const std::vector<StmtPtr>& body, bool /*fnBody*/) {
        // function bodies are called with depth already pushed; block statements here
        int savedTmp = tmp_;
        std::string v = genBlockValue(o, body, 2);
        tmp_ = savedTmp;
        lastBlockValue_ = v;
    }

    // Generate a statement list; returns the expression holding the block's
    // trailing value (Forge implicit returns), or "" when there is none.
    std::string genBlockValue(std::ostringstream& o, const std::vector<StmtPtr>& stmts, int depth) {
        scopes_.push_back({});
        std::string val;
        for (size_t i = 0; i < stmts.size(); i++) {
            bool last = (i + 1 == stmts.size());
            val = genStmtValue(o, stmts[i], depth, last);
        }
        scopes_.pop_back();
        return val;
    }

    // last=true: capture trailing expression / if-expression values
    std::string genStmtValue(std::ostringstream& o, const StmtPtr& s, int depth, bool last) {
        if (last) {
            if (auto* e = std::get_if<ExprStmt>(&s->node)) {
                std::string v = tmp("e");
                indent(o, depth);
                o << "Object " << v << " = " << expr(e->expression) << ";\n";
                return v;
            }
            if (auto* i = std::get_if<IfStmt>(&s->node)) {
                std::string iv = tmp("iv");
                indent(o, depth);
                o << "Object " << iv << ";\n";
                indent(o, depth);
                o << "if (R.truth(" << expr(i->condition) << ")) {\n";
                std::string tv = genBlockValue(o, i->thenBranch, depth + 1);
                indent(o, depth + 1);
                o << iv << " = " << (tv.empty() ? "null" : tv) << ";\n";
                indent(o, depth);
                if (!i->elseBranch.empty()) {
                    o << "} else {\n";
                    std::string ev = genBlockValue(o, i->elseBranch, depth + 1);
                    indent(o, depth + 1);
                    o << iv << " = " << (ev.empty() ? "null" : ev) << ";\n";
                    indent(o, depth);
                } else {
                    o << "} else {\n";
                    indent(o, depth + 1);
                    o << iv << " = null;\n";
                    indent(o, depth);
                }
                o << "}\n";
                return iv;
            }
            // (a trailing match/if-expression arrives wrapped in ExprStmt and
            //  is handled by the first branch above)
        }
        genStmt(o, s, depth);
        return "";
    }

    std::string lastBlockValue_;

    // generate statement list; `depth` = indent level (in units of 4 spaces)
    void genStmts(std::ostringstream& o, const std::vector<StmtPtr>& stmts, int depth) {
        // nested statements are never script top-level: a `let` inside a
        // block or loop is a local (R.Cell), not a static field
        bool savedTop = scriptTopLevel_;
        scriptTopLevel_ = false;
        scopes_.push_back({});
        for (auto& s : stmts) genStmt(o, s, depth);
        scopes_.pop_back();
        scriptTopLevel_ = savedTop;
    }

    void genStmt(std::ostringstream& o, const StmtPtr& s, int depth) {
        if (auto* e = std::get_if<ExprStmt>(&s->node)) {
            indent(o, depth);
            // Java has no bare-expression statements; assign to a scratch
            // local so any Forge expression statement stays compilable.
            o << "Object __e" << nextTmp() << " = " << expr(e->expression) << ";\n";
        } else if (auto* v = std::get_if<VarDecl>(&s->node)) {
            indent(o, depth);
            if (scriptTopLevel_ && !isLocal(v->name)) {
                // top-level-in-main: static field assignment
                o << sanitizeIdent(v->name) << " = "
                  << (v->initializer ? expr(v->initializer) : "null") << ";\n";
            } else {
                scopes_.back().insert(v->name);
                o << "R.Cell " << sanitizeIdent(v->name) << " = new R.Cell("
                  << (v->initializer ? expr(v->initializer) : "null") << ");\n";
            }
        } else if (auto* r = std::get_if<ReturnStmt>(&s->node)) {
            indent(o, depth);
            o << "return " << (r->value ? expr(r->value) : "null") << ";\n";
        } else if (auto* i = std::get_if<IfStmt>(&s->node)) {
            indent(o, depth);
            o << "if (R.truth(" << expr(i->condition) << ")) {\n";
            genStmts(o, i->thenBranch, depth + 1);
            indent(o, depth);
            if (!i->elseBranch.empty()) {
                o << "} else {\n";
                genStmts(o, i->elseBranch, depth + 1);
                indent(o, depth);
            }
            o << "}\n";
        } else if (auto* w = std::get_if<WhileStmt>(&s->node)) {
            indent(o, depth);
            o << "while (R.truth(" << expr(w->condition) << ")) {\n";
            genStmts(o, w->body, depth + 1);
            indent(o, depth);
            o << "}\n";
        } else if (auto* f = std::get_if<ForStmt>(&s->node)) {
            std::string init, condS, inc;
            // initializer must be VarDecl
            if (f->initializer) {
                if (auto* vd = std::get_if<VarDecl>(&f->initializer->node)) {
                    scopes_.push_back({});
                    scopes_.back().insert(vd->name);
                    init = "R.Cell " + sanitizeIdent(vd->name) + " = new R.Cell("
                         + (vd->initializer ? expr(vd->initializer) : "null") + ")";
                    condS = f->condition ? expr(f->condition) : "true";
                    inc = f->increment ? expr(f->increment) : "";
                    indent(o, depth);
                    o << "for (" << init << "; R.truth(" << condS
                      << "); " << inc << ") {\n";
                    genStmtsNoScope(o, f->body, depth + 1);
                    indent(o, depth);
                    o << "}\n";
                    scopes_.pop_back();
                    return;
                }
            }
            // fallback: while-style
            indent(o, depth);
            o << "{\n";
            if (f->initializer) genStmt(o, f->initializer, depth + 1);
            o << "    while (R.truth(" << (f->condition ? expr(f->condition) : "true") << ")) {\n";
            genStmts(o, f->body, depth + 2);
            if (f->increment) { indent(o, depth + 2); o << expr(f->increment) << ";\n"; }
            indent(o, depth); o << "    }\n";
            indent(o, depth); o << "}\n";
        } else if (auto* fi = std::get_if<ForInStmt>(&s->node)) {
            std::string itv = tmp("it");
            indent(o, depth);
            o << "for (Object " << itv << " : R.iter(" << expr(fi->iterable) << ")) {\n";
            scopes_.push_back({});
            scopes_.back().insert(fi->variable);
            indent(o, depth + 1);
            o << "R.Cell " << sanitizeIdent(fi->variable) << " = new R.Cell(" << itv << ");\n";
            genStmtsNoScope(o, fi->body, depth + 1);
            indent(o, depth);
            o << "}\n";
            scopes_.pop_back();
        } else if (std::get_if<BreakStmt>(&s->node)) {
            indent(o, depth); o << "break;\n";
        } else if (std::get_if<ContinueStmt>(&s->node)) {
            indent(o, depth); o << "continue;\n";
        } else if (auto* b = std::get_if<BlockStmt>(&s->node)) {
            indent(o, depth); o << "{\n";
            genStmts(o, b->statements, depth + 1);
            indent(o, depth); o << "}\n";
        } else if (auto* t = std::get_if<TryStmt>(&s->node)) {
            indent(o, depth);
            o << "try {\n";
            genStmts(o, t->body, depth + 1);
            indent(o, depth);
            o << "} catch (RuntimeException e) {\n";
            scopes_.push_back({});
            scopes_.back().insert(t->catchVar);
            indent(o, depth + 1);
            o << "R.Cell " << sanitizeIdent(t->catchVar) << " = new R.Cell(e.getMessage());\n";
            genStmtsNoScope(o, t->catchBody, depth + 1);
            scopes_.pop_back();
            indent(o, depth);
            o << "}\n";
        } else if (auto* fd = std::get_if<FnDecl>(&s->node)) {
            // nested fn decl: hoist as local closure value
            indent(o, depth);
            std::string fname = sanitizeIdent(fd->name);
            o << "R.Fn " << fname << " = " << lambdaForFn(fd->params, fd->body, fd->name) << ";\n";
            scopes_.back().insert(fd->name);
        } else if (auto* im = std::get_if<ImportStmt>(&s->node)) {
            // selective imports become local bindings
            if (!im->selectiveImports.empty()) {
                for (auto& sel : im->selectiveImports) {
                    std::string target = moduleBuiltin(im->moduleName, sel);
                    if (target.empty())
                        throw CompileError("module '" + im->moduleName + "' member '" + sel +
                                           "' not supported by java backend", s->line);
                    indent(o, depth);
                    o << "R.Fn " << sanitizeIdent(sel) << " = " << target << ";\n";
                    scopes_.back().insert(sel);
                }
            }
        } else if (auto* ed = std::get_if<EnumDecl>(&s->node)) {
            for (size_t i = 0; i < ed->variants.size(); i++) {
                indent(o, depth);
                o << sanitizeIdent(ed->variants[i]) << " = R.integer(" << i << ");\n";
            }
        } else if (auto* sd = std::get_if<StructDecl>(&s->node)) {
            (void)sd; // structs are data-only; nothing needed at runtime
        } else if (auto* idl = std::get_if<ImplDecl>(&s->node)) {
            for (auto& m : idl->methods) {
                FnDecl fd{m.name, m.params, m.body};
                std::ostringstream nm;
                nm << idl->className << "_" << m.name;
                // register like class methods on existing class name
                indent(o, depth);
                o << "R.CLASSES.put(\"" << escapeJava(idl->className) << "."
                  << escapeJava(m.name) << "\", " << lambdaForMethod(idl->className, m) << ");\n";
            }
        } else if (std::get_if<ClassDecl>(&s->node) ||
                   std::get_if<GenDecl>(&s->node) ||
                   std::get_if<ExternFnDecl>(&s->node)) {
            // handled at member level / unsupported
            if (std::get_if<GenDecl>(&s->node))
                throw CompileError("generators are not supported by the java backend yet", s->line);
        }
    }

    void genStmtsNoScope(std::ostringstream& o, const std::vector<StmtPtr>& stmts, int depth) {
        bool savedTop = scriptTopLevel_;
        scriptTopLevel_ = false;
        for (auto& s : stmts) genStmt(o, s, depth);
        scriptTopLevel_ = savedTop;
    }

    // ---------------- expressions ----------------

    std::string builtinCall(const std::string& name, const std::vector<ExprPtr>& args) {
        std::vector<std::string> a;
        for (auto& x : args) a.push_back(expr(x));
        auto join = [&]() {
            std::string s;
            for (size_t i = 0; i < a.size(); i++) { if (i) s += ", "; s += a[i]; }
            return s;
        };
        if (name == "print") return "R.print(" + join() + ")";
        if (name == "len") return "R.len(" + join() + ")";
        if (name == "str") return "R.str(" + join() + ")";
        if (name == "int") return "R.toInt(" + join() + ")";
        if (name == "float") return "R.toFloatV(" + join() + ")";
        if (name == "type") return "R.typeOf(" + join() + ")";
        if (name == "abs") return "R.abs(" + join() + ")";
        if (name == "min") return "R.min(" + join() + ")";
        if (name == "max") return "R.max(" + join() + ")";
        if (name == "sqrt") return "R.sqrt(" + join() + ")";
        if (name == "pow") return "R.pow(" + join() + ")";
        if (name == "floor") return "R.floor(" + join() + ")";
        if (name == "ceil") return "R.ceil(" + join() + ")";
        if (name == "round") return "R.round(" + join() + ")";
        if (name == "random") return "R.random()";
        if (name == "randomInt") return "R.randomInt(" + join() + ")";
        if (name == "keys") return "R.keys(" + join() + ")";
        if (name == "values") return "R.values(" + join() + ")";
        if (name == "has") return "R.has(" + join() + ")";
        if (name == "entries") return "R.entries(" + join() + ")";
        if (name == "clone") return "R.cloneV(" + join() + ")";
        if (name == "error") return "R.errorFn(" + join() + ")";
        if (name == "push") return "R.push(" + join() + ")";
        if (name == "upper" || name == "lower" || name == "trim")
            return "R.mcall(" + (a.size() ? a[0] : "null") + ", \"" + name + "\", Arrays.asList())";
        return "";
    }

    std::string moduleBuiltin(const std::string& mod, const std::string& fn) {
        if (mod == "io") {
            if (fn == "read") return "(a) -> R.ioRead(a)";
            if (fn == "write") return "(a) -> R.ioWrite(a)";
            if (fn == "exists") return "(a) -> R.ioExists(a)";
        } else if (mod == "os") {
            if (fn == "time") return "(a) -> R.osTime(a)";
            if (fn == "env") return "(a) -> R.osEnv(a)";
            if (fn == "args") return "(a) -> R.ARGS";
            if (fn == "cwd") return "(a) -> java.nio.file.Paths.get(\"\").toAbsolutePath().toString()";
        } else if (mod == "system") {
            if (fn == "version") return "(a) -> R.systemVersion(a)";
        } else if (mod == "gui") {
            // the gui module is implemented by the host: ForgeGui (Android,
            // backed by the liquid glass library) or a no-op stub on the JVM.
            static const char* kGuiFns[] = {
                "window", "button", "label", "textfield", "checkbox", "progress",
                "panel", "slider", "gauge", "toggle", "radio", "tabs", "list",
                "combo", "set_text", "get_text", "set_checked", "set_progress",
                "set_value", "get_value", "set_items", "select", "get_selected",
                "close", "tick", "poll", "screenshot",
            };
            for (const char* g : kGuiFns)
                if (fn == g) return "(a) -> ForgeGui." + fn + "(a)";
        }
        return "";
    }

    std::string callArgs(const std::vector<ExprPtr>& args) {
        std::string s = "Arrays.asList(";
        for (size_t i = 0; i < args.size(); i++) {
            if (i) s += ", ";
            s += expr(args[i]);
        }
        s += ")";
        return s;
    }

    std::string lambdaForFn(const std::vector<std::string>& params,
                            const std::vector<StmtPtr>& body,
                            const std::string& dbgName) {
        // Build an anonymous R.Fn whose call() executes the body with Cell params.
        std::ostringstream o;
        o << "new R.Fn() { public Object call(java.util.List<Object> __a) {\n";
        scopes_.push_back({});
        auto& sc = scopes_.back();
        for (size_t i = 0; i < params.size(); i++) {
            sc.insert(params[i]);
            o << "        R.Cell " << sanitizeIdent(params[i]) << " = new R.Cell(__a.size() > "
              << i << " ? __a.get(" << i << ") : null);\n";
        }
        int savedTmp = tmp_;
        std::string val;
        for (size_t i = 0; i < body.size(); i++) {
            val = genStmtValue(o, body[i], 0, i + 1 == body.size());
        }
        tmp_ = savedTmp;
        scopes_.pop_back();
        if (!val.empty()) {
            o << "        return " << val << ";\n    } } /* " << dbgName << " */";
        } else if (bodyNeverFallsThrough(body)) {
            o << "    } } /* " << dbgName << " */";
        } else {
            o << "        return null;\n    } } /* " << dbgName << " */";
        }
        return o.str();
    }

    std::string lambdaForMethod(const std::string& cname, const FnDecl& m) {
        std::ostringstream o;
        o << "new R.Fn() { @SuppressWarnings(\"unchecked\") public Object call(java.util.List<Object> __a) {\n"
          << "            R.Obj __self = __a.isEmpty() ? null : (R.Obj)__a.get(0);\n"
          << "            return " << sanitizeIdent(cname) << "_" << sanitizeIdent(m.name)
          << "(__self, __a.subList(1, __a.size()).toArray(new Object[0]));\n"
          << "        } }";
        return o.str();
    }

    std::string expr(const ExprPtr& e) {
        if (!e) return "null";
        if (auto* x = std::get_if<IntegerLiteral>(&e->node)) {
            return "Long.valueOf(" + std::to_string(x->value) + "L)";
        }
        if (auto* x = std::get_if<FloatLiteral>(&e->node)) {
            std::ostringstream d; d.precision(17); d << std::scientific << x->value;
            return "Double.valueOf(" + d.str() + ")";
        }
        if (auto* x = std::get_if<StringLiteral>(&e->node))
            return "\"" + escapeJava(x->value) + "\"";
        if (auto* x = std::get_if<CharLiteral>(&e->node))
            return "R.str(\"" + escapeJava(std::string(1, x->value)) + "\")";
        if (auto* b = std::get_if<BoolLiteral>(&e->node))
            return b->value ? "Boolean.TRUE" : "Boolean.FALSE";
        if (std::get_if<NilLiteral>(&e->node)) return "null";
        if (std::get_if<ThisLiteral>(&e->node)) return "th.v";
        if (auto* x = std::get_if<Identifier>(&e->node)) {
            const std::string& n = x->name;
            if (n == "true") return "Boolean.TRUE";
            if (n == "false") return "Boolean.FALSE";
            if (n == "nil") return "null";
            if (isLocal(n)) return sanitizeIdent(n) + ".v";
            if (globalFns_.count(n)) {
                // first-class function reference
                return "new R.Fn() { public Object call(java.util.List<Object> a) { return "
                       + sanitizeIdent(n) + "(a.toArray(new Object[0])); } }";
            }
            if (globalVars_.count(n)) return sanitizeIdent(n);
            throw CompileError("undefined variable '" + n + "'", e->line);
        }
        if (auto* x = std::get_if<BinaryExpr>(&e->node)) {
            std::string L = expr(x->left), Rr = expr(x->right);
            switch (x->op.type) {
                case TokenType::PLUS: return "R.add(" + L + ", " + Rr + ")";
                case TokenType::MINUS: return "R.sub(" + L + ", " + Rr + ")";
                case TokenType::STAR: return "R.mul(" + L + ", " + Rr + ")";
                case TokenType::SLASH: return "R.div(" + L + ", " + Rr + ")";
                case TokenType::PERCENT: return "R.mod(" + L + ", " + Rr + ")";
                case TokenType::EQUAL_EQUAL: return "R.eq(" + L + ", " + Rr + ")";
                case TokenType::BANG_EQUAL: return "R.neq(" + L + ", " + Rr + ")";
                case TokenType::LESS: return "R.lt(" + L + ", " + Rr + ")";
                case TokenType::LESS_EQUAL: return "R.le(" + L + ", " + Rr + ")";
                case TokenType::GREATER: return "R.gt(" + L + ", " + Rr + ")";
                case TokenType::GREATER_EQUAL: return "R.ge(" + L + ", " + Rr + ")";
                case TokenType::AND: return "(R.truth(" + L + ") && R.truth(" + Rr + "))";
                case TokenType::OR: return "(R.truth(" + L + ") || R.truth(" + Rr + "))";
                case TokenType::AMPERSAND: return "R.band(" + L + ", " + Rr + ")";
                case TokenType::BAR: return "R.bor(" + L + ", " + Rr + ")";
                case TokenType::CARET: return "R.bxor(" + L + ", " + Rr + ")";
                default: throw CompileError("unsupported operator", e->line);
            }
        }
        if (auto* x = std::get_if<UnaryExpr>(&e->node)) {
            std::string V = expr(x->operand);
            switch (x->op.type) {
                case TokenType::MINUS: return "R.neg(" + V + ")";
                case TokenType::NOT: return "R.not(" + V + ")";
                default: throw CompileError("unsupported unary operator", e->line);
            }
        }
        if (auto* x = std::get_if<TernaryExpr>(&e->node))
            return "(R.truth(" + expr(x->condition) + ") ? " + expr(x->thenExpr)
                   + " : " + expr(x->elseExpr) + ")";
        if (auto* x = std::get_if<CallExpr>(&e->node)) {
            if (auto* id = std::get_if<Identifier>(&x->callee->node)) {
                // constructor call: ClassName(args...)
                if (classNames_.count(id->name))
                    return "R.newInst(\"" + escapeJava(id->name) + "\", "
                           + callArgs(x->arguments) + ")";
                std::string b = builtinCall(id->name, x->arguments);
                if (!b.empty()) return b;
                if (globalFns_.count(id->name) && !isLocal(id->name)) {
                    std::string s = sanitizeIdent(id->name) + "(";
                    for (size_t i = 0; i < x->arguments.size(); i++) {
                        if (i) s += ", ";
                        s += expr(x->arguments[i]);
                    }
                    return s + ")";
                }
                return "R.call(" + expr(x->callee) + ", " + callArgs(x->arguments) + ")";
            }
            if (auto* se = std::get_if<SuperExpr>(&x->callee->node)) {
                return "R.superCall(th.v, \"" + escapeJava(se->method) + "\", "
                       + callArgs(x->arguments) + ")";
            }
            if (auto* me = std::get_if<MemberExpr>(&x->callee->node)) {
                // method call: obj.member(args)
                if (auto* baseId = std::get_if<Identifier>(&me->object->node)) {
                    // module-qualified builtin? io.read(...), gui.button(...) etc.
                    // The module name may come from `import "x"` (the alias is the
                    // module name) - known modules are mapped to their runtime
                    // helper, unknown names fall through to a normal method call.
                    if (!globalVars_.count(baseId->name) &&
                        !classNames_.count(baseId->name) && !isLocal(baseId->name)) {
                        std::string mb = moduleBuiltin(baseId->name, me->member);
                        if (!mb.empty())
                            return "R.call((R.Fn)" + mb + ", " + callArgs(x->arguments) + ")";
                    }
                }
                std::string argsStr;
                {
                    std::vector<std::string> a;
                    for (auto& ax : x->arguments) a.push_back(expr(ax));
                    argsStr = "Arrays.asList(";
                    for (size_t i = 0; i < a.size(); i++) { if (i) argsStr += ", "; argsStr += a[i]; }
                    argsStr += ")";
                }
                return "R.mcall(" + expr(me->object) + ", \"" + escapeJava(me->member) + "\", "
                       + argsStr + ")";
            }
            return "R.call(" + expr(x->callee) + ", " + callArgs(x->arguments) + ")";
        }
        if (auto* x = std::get_if<MemberExpr>(&e->node))
            return "R.get(" + expr(x->object) + ", \"" + escapeJava(x->member) + "\")";
        if (auto* x = std::get_if<IndexExpr>(&e->node))
            return "R.idx(" + expr(x->object) + ", " + expr(x->index) + ")";
        if (auto* x = std::get_if<ArrayLiteral>(&e->node)) {
            std::string s = "R.arr(";
            for (size_t i = 0; i < x->elements.size(); i++) {
                if (i) s += ", ";
                s += expr(x->elements[i]);
            }
            return s + ")";
        }
        if (auto* x = std::get_if<MapLiteral>(&e->node)) {
            std::string s = "R.map(";
            for (size_t i = 0; i < x->entries.size(); i++) {
                if (i) s += ", ";
                s += expr(x->entries[i].key) + ", " + expr(x->entries[i].value);
            }
            return s + ")";
        }
        if (auto* x = std::get_if<FnExpr>(&e->node))
            return lambdaForFn(x->params, x->body, "<closure>");
        if (auto* x = std::get_if<AssignExpr>(&e->node)) {
            std::string V = expr(x->value);
            const std::string& n = x->name;
            if (isLocal(n)) return sanitizeIdent(n) + ".v = " + V;
            if (globalVars_.count(n)) return sanitizeIdent(n) + " = " + V;
            throw CompileError("assignment to undefined variable '" + n + "'", e->line);
        }
        if (auto* x = std::get_if<AssignMemberExpr>(&e->node))
            return "R.set(" + expr(x->object) + ", \"" + escapeJava(x->member) + "\", "
                   + expr(x->value) + ")";
        if (auto* x = std::get_if<AssignIndexExpr>(&e->node))
            return "R.setIdx(" + expr(x->object) + ", " + expr(x->index) + ", "
                   + expr(x->value) + ")";
        if (auto* x = std::get_if<StringInterpExpr>(&e->node)) {
            std::string s;
            bool first = true;
            for (auto& p : x->parts) {
                std::string piece = p.isExpr ? "R.str(" + expr(p.expression) + ")"
                                             : "\"" + escapeJava(p.text) + "\"";
                if (first) { s = piece; first = false; }
                else s += " + " + piece;
            }
            return "(" + (first ? "\"\"" : s) + ")";
        }
        if (auto* x = std::get_if<ThrowExpr>(&e->node))
            return "throwErr(" + expr(x->value) + ")";
        if (std::get_if<YieldExpr>(&e->node))
            throw CompileError("yield is only valid inside generators (unsupported by java backend)", e->line);
        if (std::get_if<SuperExpr>(&e->node))
            throw CompileError("bare super expression not supported; use super.method(...) calls", e->line);
        if (auto* x = std::get_if<MatchExpr>(&e->node)) return matchExpr(e->line, *x);
        throw CompileError("unsupported expression in java backend", e->line);
    }

    // match expression translated into an if-chain inside an immediately-invoked Fn
    std::string matchExpr(int line, const MatchExpr& mx) {
        std::string rv = tmp("mv");
        std::string valV = tmp("mval");
        std::ostringstream o;
        o << "new R.Fn() { @SuppressWarnings(\"unchecked\") public Object call(java.util.List<Object> __ignored) {\n"
          << "            Object " << valV << " = null;\n"
          << "            Object " << rv << " = null;\n";
        // pre-declare binding cells for identifier/guard patterns
        std::unordered_set<std::string> bound;
        std::function<void(const MatchPattern&)> collect = [&](const MatchPattern& p) {
            if (p.type == MatchPattern::IDENTIFIER || p.type == MatchPattern::GUARD)
                if (!p.name.empty()) bound.insert(p.name);
            for (auto& e : p.elements) collect(e);
        };
        for (auto& mc : mx.cases) collect(mc.pattern);
        scopes_.push_back({});
        for (auto& n : bound) {
            scopes_.back().insert(n);
            o << "            R.Cell " << sanitizeIdent(n) << " = new R.Cell(null);\n";
        }
        // evaluate the matched value once
        o << "            " << valV << " = " << expr(mx.value) << ";\n";
        for (auto& mc : mx.cases) {
            o << "            if (" << matchCond(valV, mc.pattern) << ") {\n";
            genStmts(o, mc.body, 4);
            if (!mc.body.empty()) {
                if (auto* lastE = std::get_if<ExprStmt>(&mc.body.back()->node)) {
                    o << "                " << rv << " = " << expr(lastE->expression) << ";\n";
                }
            }
            o << "            } else ";
        }
        o << "{\n                throw new R.ForgeError(\"match: no matching case (line "
          << line << ")\");\n            }\n"
          << "            return " << rv << ";\n"
          << "        } }.call(Arrays.asList())";
        scopes_.pop_back();
        return o.str();
    }

    std::string matchCond(const std::string& valV, const MatchPattern& p) {
        switch (p.type) {
            case MatchPattern::WILDCARD: return "true";
            case MatchPattern::LITERAL:
                return "R.eq(" + valV + ", " + (p.literal ? expr(p.literal) : "null") + ")";
            case MatchPattern::IDENTIFIER:
                return "(" + sanitizeIdent(p.name) + ".v = " + valV + ") != null || true";
            case MatchPattern::GUARD: {
                // bind first (if there is a binding), then test the guard
                std::string cond = "true";
                if (!p.name.empty()) {
                    std::string bind = sanitizeIdent(p.name) + ".v = " + valV;
                    cond = "(" + bind + ") != null || true";
                }
                return "(" + cond + ") && R.truth(" + expr(p.guardCondition) + ")";
            }
            case MatchPattern::RANGE:
                return "(R.isNum(" + valV + ") && R.num(" + valV + ") >= R.num("
                       + expr(p.rangeLow) + ") && R.num(" + valV + ") <= R.num("
                       + expr(p.rangeHigh) + "))";
            case MatchPattern::ARRAY_DESTRUCTURE: {
                std::string c = "(" + valV + " instanceof ArrayList)";
                if (!p.hasRest && !p.elements.empty()) {
                    c += " && ((ArrayList)" + valV + ").size() == "
                         + std::to_string(p.elements.size());
                } else {
                    c += " && ((ArrayList)" + valV + ").size() >= "
                         + std::to_string(p.elements.size());
                }
                return c;
            }
        }
        return "false";
    }

    void emitMain() {
        out_ << members_.str();
        out_ << "    public static Object throwErr(Object v) { return R.errorFn(v); }\n\n"
             << "    @SuppressWarnings(\"unchecked\")\n"
             << "    public static void main(String[] argv) {\n"
             << "        R.ARGS = Arrays.asList((Object[])argv);\n"
             << "        registerClasses();\n"
             << "        try {\n"
             << "            runScript();\n"
             << "        } catch (R.ForgeError fe) {\n"
             << "            System.err.println(\"Uncaught exception: \" + fe.getMessage());\n"
             << "            System.exit(1);\n"
             << "        }\n"
             << "    }\n\n"
             << "    private static void runScript() throws R.ForgeError {\n";
        scriptTopLevel_ = true;
        scopes_.push_back({});
        for (auto& s : program_) {
            // declarations that became class members are skipped in main()
            if (std::get_if<FnDecl>(&s->node) || std::get_if<ClassDecl>(&s->node) ||
                std::get_if<GenDecl>(&s->node) || std::get_if<ExternFnDecl>(&s->node))
                continue;
            genStmt(out_, s, 2);
        }
        scopes_.pop_back();
        scriptTopLevel_ = false;
        out_ << "    }\n";
    }
};

} // anonymous namespace

// ============================================================
// Public API
// ============================================================

std::string transpileToJava(const std::vector<std::shared_ptr<Stmt>>& program,
                            const std::string& mainClassName) {
    Gen g(program, mainClassName);
    return g.generate();
}

int java_main(int argc, char** argv) {
    std::string srcFile, outFile, mainName;
    bool keep = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-o") {
            if (i + 1 >= argc) { std::cerr << "Error: -o requires a path\n"; return 1; }
            outFile = argv[++i];
        } else if (a == "--main") {
            if (i + 1 >= argc) { std::cerr << "Error: --main requires a name\n"; return 1; }
            mainName = argv[++i];
        } else if (a == "--keep") {
            keep = true;
        } else if (a == "--help" || a == "-h") {
            std::cout << "Usage: forgevm java <file.fge> [-o out.jar] [--main ClassName] [--keep]\n\n"
                      << "Transpiles Forge source to Java, compiles it with javac and packages a\n"
                      << "runnable .jar (requires JDK on PATH or JAVA_HOME). With --keep the generated\n"
                      << ".java files are kept next to the output.\n";
            return 0;
        } else if (srcFile.empty()) {
            srcFile = a;
        }
    }
    if (srcFile.empty()) {
        std::cerr << "Error: forgevm java requires a .fge file (see --help)\n";
        return 1;
    }
    if (mainName.empty()) {
        mainName = fs::path(srcFile).stem().string();
        if (mainName.empty()) mainName = "ForgeMain";
        if (std::isdigit((unsigned char)mainName[0])) mainName = "_" + mainName;
    }
    // sanitize: alnum/underscore only — kills manifest newline injection and
    // path traversal via --main before it reaches filenames or the shell
    mainName = sanitizeIdent("Forge_" + mainName);

    std::ifstream in(srcFile);
    if (!in.is_open()) {
        std::cerr << "Error: cannot open " << srcFile << "\n";
        return 1;
    }
    std::stringstream buf;
    buf << in.rdbuf();

    try {
        Lexer lexer(buf.str());
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();

        std::string javaSrc = transpileToJava(program, mainName);

        fs::path outPath(outFile.empty() ? srcFile : outFile);
        fs::path buildDir = outPath.parent_path().empty()
            ? fs::path("forge-java-build") : outPath.parent_path() / "forge-java-build";
        fs::create_directories(buildDir);
        fs::path javaFile = buildDir / (mainName + ".java");
        {
            std::ofstream jf(javaFile);
            jf << javaSrc;
        }
        std::cout << "Transpiled " << srcFile << " -> " << javaFile.string() << "\n";

        // programs that use the `gui` module need the no-op desktop stub
        fs::path stubFile;
        bool needsGuiStub = javaSrc.find("ForgeGui.") != std::string::npos;
        if (needsGuiStub) {
            stubFile = buildDir / "ForgeGui.java";
            std::ofstream sf(stubFile);
            sf << forge::androidcmd::TPL_ForgeGuiStub_java;
        }

        // locate JDK tools
        auto findTool = [](const char* tool) -> std::string {
            std::string home = std::getenv("JAVA_HOME") ? std::getenv("JAVA_HOME") : "";
            if (!home.empty()) {
                fs::path p = fs::path(home) / "bin" / tool;
                std::error_code ec;
                if (fs::exists(p, ec)) return p.string();
            }
            std::string probe = std::string("which ") + tool + " >/dev/null 2>&1";
            if (std::system(probe.c_str()) == 0) return tool;
            return "";
        };
        std::string javac = findTool("javac");
        std::string jar = findTool("jar");
        bool wantJar = !outFile.empty() && fs::path(outFile).extension() == ".jar";

        if (javac.empty() || (wantJar && jar.empty())) {
            std::cout << "JDK tools not found"
                      << (wantJar ? " (need javac + jar)" : " (need javac)")
                      << " — set JAVA_HOME or install a JDK.\n"
                      << "Generated Java kept at: " << javaFile.string() << "\n";
            return 0;
        }

        std::string compileCmd = shellQuote(javac) + " -Xlint:-options " + shellQuote(javaFile.string());
        if (needsGuiStub) compileCmd += " " + shellQuote(stubFile.string());
        int rc = std::system(compileCmd.c_str());
        if (rc != 0) {
            std::cerr << "Error: javac failed\n";
            return 1;
        }

        if (!wantJar) {
            std::cout << "Compiled. Run with: java -cp " << buildDir.string()
                      << " " << mainName << "\n";
            if (!keep)
                std::cout << "(use -o out.jar to package a runnable .jar)\n";
            return 0;
        }

        fs::path manifest = buildDir / "manifest.mf";
        {
            std::ofstream mf(manifest);
            mf << "Main-Class: " << mainName << "\n";
        }
        std::string jarCmd = shellQuote(jar) + " cfm " + shellQuote(outPath.string()) + " " + shellQuote(manifest.string())
            + " -C " + shellQuote(buildDir.string()) + " ." ;
        // (jars every class in buildDir — includes the R runtime nested classes)
        rc = std::system(jarCmd.c_str());
        if (rc != 0) {
            std::cerr << "Error: jar failed\n";
            return 1;
        }
        if (!keep) {
            std::error_code ec;
            fs::remove(javaFile, ec);
            fs::remove(manifest, ec);
            if (needsGuiStub) fs::remove(stubFile, ec);
            for (auto& entry : fs::directory_iterator(buildDir))
                if (entry.path().extension() == ".class") fs::remove(entry.path(), ec);
        }
        std::cout << "Packaged " << outPath.string() << "\n"
                  << "Run with: java -jar " << outPath.string() << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Java backend error: " << e.what() << "\n";
        return 1;
    }
}

} // namespace forge::javabackend
