// c2forge - Translate C/C++ to Forge (.fge)
// Minimal translator for common C/C++ patterns

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include <algorithm>
#include <filesystem>

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string translate(const std::string& code) {
    std::string out = code;
    
    // Remove includes
    out = std::regex_replace(out, std::regex("#include\\s+<[^>]+>\\s*"), "");
    out = std::regex_replace(out, std::regex("#include\\s+\"[^\"]+\"\\s*"), "");
    
    // using namespace
    out = std::regex_replace(out, std::regex("using\\s+namespace\\s+[^;]+;\\s*"), "");
    
    // printf/scanf basics
    out = std::regex_replace(out, std::regex("printf\\s*\\("), "print(");
    out = std::regex_replace(out, std::regex("cout\\s*<<"), "print(");
    
    // int/void main
    out = std::regex_replace(out, std::regex("\\bint\\s+main\\s*\\("), "fn main(");
    out = std::regex_replace(out, std::regex("\\bvoid\\s+main\\s*\\("), "fn main(");
    out = std::regex_replace(out, std::regex("\\bint\\s+(\\w+)\\s*\\("), "fn $1(");
    out = std::regex_replace(out, std::regex("\\bvoid\\s+(\\w+)\\s*\\("), "fn $1(");
    out = std::regex_replace(out, std::regex("\\bauto\\s+(\\w+)"), "let $1");
    
    // types
    out = std::regex_replace(out, std::regex("\\bint\\s+(\\w+)"), "let $1");
    out = std::regex_replace(out, std::regex("\\bfloat\\s+(\\w+)"), "let $1");
    out = std::regex_replace(out, std::regex("\\bdouble\\s+(\\w+)"), "let $1");
    out = std::regex_replace(out, std::regex("\\bchar\\*\\s+(\\w+)"), "let $1");
    out = std::regex_replace(out, std::regex("\\bstd::string\\s+(\\w+)"), "let $1");
    out = std::regex_replace(out, std::regex("\\bbool\\s+(\\w+)"), "let $1");
    
    // for loops
    out = std::regex_replace(out, std::regex("for\\s*\\(\\s*int\\s+(\\w+)\\s*=\\s*([^;]+);\\s*\\1\\s*<\\s*([^;]+);\\s*\\1\\+\\+\\s*\\)"), "for (let $1 = $2; $1 < $3; $1 = $1 + 1) {");
    
    // nullptr/NULL
    out = std::regex_replace(out, std::regex("\\bnullptr\\b"), "nil");
    out = std::regex_replace(out, std::regex("\\bNULL\\b"), "nil");
    
    // return 0
    out = std::regex_replace(out, std::regex("return\\s+0\\s*;"), "return;");
    
    // Remove semicolons at end of lines (basic)
    // This is a simplified translator
    
    std::stringstream result;
    result << "// Translated by c2forge from C/C++\n";
    result << "// WARNING: This is a rough translation - manual review required\n\n";
    result << out;
    
    return result.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "c2forge - Translate C/C++ to Forge (.fge)\n\n";
        std::cout << "Usage: c2forge <input.c|.cpp> [output.fge]\n";
        std::cout << "       c2forge --dir <directory>\n";
        return 1;
    }

    std::string input = argv[1];
    std::string output;

    if (input == "--dir") {
        std::cout << "[c2forge] Directory mode not fully implemented yet\n";
        return 0;
    }

    if (argc >= 3) {
        output = argv[2];
    } else {
        size_t dot = input.find_last_of(".");
        if (dot != std::string::npos) {
            output = input.substr(0, dot) + ".fge";
        } else {
            output = input + ".fge";
        }
    }

    std::string code = readFile(input);
    if (code.empty()) {
        std::cerr << "[c2forge] Error: Cannot read " << input << "\n";
        return 1;
    }

    std::string translated = translate(code);

    std::ofstream out(output);
    if (!out) {
        std::cerr << "[c2forge] Error: Cannot write " << output << "\n";
        return 1;
    }
    out << translated;

    std::cout << "[c2forge] " << input << " -> " << output << "\n";
    std::cout << "[c2forge] Note: Manual review and adjustment required\n";

    // Vice versa: register the new component in the single project file
    // (Forgefile preferred; legacy ForgeLists.txt created by CFMAKE also works).
    std::string projectFile;
    if (std::filesystem::exists("Forgefile")) projectFile = "Forgefile";
    else if (std::filesystem::exists("ForgeLists.txt")) projectFile = "ForgeLists.txt";
    if (!projectFile.empty()) {
        std::ifstream pf(projectFile);
        std::string contents((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());
        pf.close();
        if (contents.find("component(" + output + ")") == std::string::npos &&
            contents.find("file(" + output + ")") == std::string::npos) {
            std::ofstream pf2(projectFile, std::ios::app);
            pf2 << "\n# added by c2forge (hand-tune)\ncomponent(" << output << ")\n";
            pf2.close();
            std::cout << "[c2forge] Registered in " << projectFile << ": component(" << output << ")\n";
        }
    }

    return 0;
}
