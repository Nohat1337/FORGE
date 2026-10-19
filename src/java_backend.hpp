#pragma once

// ============================================================
// Forge -> Java backend
//
// Transpiles a parsed Forge program (.fge AST) into a single
// self-contained Java source file that can be compiled with
// javac and packaged into a runnable .jar. The generated jar
// runs anywhere a JVM runs -- including as library code inside
// larger Java applications (e.g. game mod toolchains).
//
// Value model (Forge dynamic values on the JVM):
//   nil    -> null
//   bool   -> java.lang.Boolean
//   int    -> java.lang.Long
//   float  -> java.lang.Double
//   string -> java.lang.String
//   array  -> java.util.ArrayList<Object>
//   map    -> java.util.LinkedHashMap<String,Object>
//   fn     -> R.Fn (interface)
//   inst   -> R.Obj (registry-backed class instance)
// ============================================================

#include <string>
#include <vector>
#include <memory>

struct Stmt;

namespace forge::javabackend {

// Transpile a program AST into Java source for class `mainClassName`.
std::string transpileToJava(const std::vector<std::shared_ptr<Stmt>>& program,
                            const std::string& mainClassName);

// CLI entry: forgevm java <file.fge> [-o out.jar] [--main Name] [--keep]
int java_main(int argc, char** argv);

} // namespace forge::javabackend
