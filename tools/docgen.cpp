#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>

struct DocEntry {
    std::string type;
    std::string name;
    std::string signature;
    std::string docComment;
    int line;
};

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> splitLines(const std::string& source) {
    std::vector<std::string> lines;
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::vector<DocEntry> extractDocs(const std::string& source) {
    std::vector<DocEntry> entries;
    auto lines = splitLines(source);

    std::vector<std::string> pendingComments;

    for (size_t i = 0; i < lines.size(); i++) {
        std::string trimmed = trim(lines[i]);

        // Collect comments above declarations
        if (trimmed.substr(0, 2) == "//") {
            std::string comment = trim(trimmed.substr(2));
            if (comment.substr(0, 1) == "!") {
                pendingComments.push_back(comment.substr(1));
            } else {
                pendingComments.push_back(comment);
            }
            continue;
        }

        // Skip blank lines (but preserve pending comments)
        if (trimmed.empty()) {
            continue;
        }

        // Check for function declarations: fn name(params) or fn name(params) {
        std::regex fnRegex(R"(^(?:pub\s+)?fn\s+(\w+)\s*\(([^)]*)\))");
        std::smatch fnMatch;
        if (std::regex_search(trimmed, fnMatch, fnRegex)) {
            DocEntry entry;
            entry.type = "function";
            entry.name = fnMatch[1];
            entry.signature = "fn " + entry.name + "(" + fnMatch[2] + ")";
            entry.line = (int)(i + 1);
            if (!pendingComments.empty()) {
                for (auto& c : pendingComments) entry.docComment += c + " ";
                pendingComments.clear();
            }
            entries.push_back(entry);
            continue;
        }

        // Check for class declarations: class Name or class Name extends Base
        std::regex classRegex(R"(^(?:pub\s+)?class\s+(\w+)(?:\s+extends\s+(\w+))?)");
        std::smatch classMatch;
        if (std::regex_search(trimmed, classMatch, classRegex)) {
            DocEntry entry;
            entry.type = "class";
            entry.name = classMatch[1];
            entry.signature = "class " + entry.name;
            if (classMatch[2].matched) entry.signature += " extends " + classMatch[2].str();
            entry.line = (int)(i + 1);
            if (!pendingComments.empty()) {
                for (auto& c : pendingComments) entry.docComment += c + " ";
                pendingComments.clear();
            }
            entries.push_back(entry);
            continue;
        }

        // Check for method declarations inside class: init(params) or name(params)
        std::regex methodRegex(R"(^\s+(?:init|(\w+))\s*\(([^)]*)\)\s*\{?)");
        std::smatch methodMatch;
        if (std::regex_search(trimmed, methodMatch, methodRegex)) {
            DocEntry entry;
            entry.type = "method";
            entry.name = methodMatch[1].matched ? methodMatch[1].str() : "init";
            entry.signature = entry.name + "(" + methodMatch[2] + ")";
            entry.line = (int)(i + 1);
            if (!pendingComments.empty()) {
                for (auto& c : pendingComments) entry.docComment += c + " ";
                pendingComments.clear();
            }
            entries.push_back(entry);
            continue;
        }

        // Check for generator declarations: gen name(params)
        std::regex genRegex(R"(^(?:pub\s+)?gen\s+(\w+)\s*\(([^)]*)\))");
        std::smatch genMatch;
        if (std::regex_search(trimmed, genMatch, genRegex)) {
            DocEntry entry;
            entry.type = "generator";
            entry.name = genMatch[1];
            entry.signature = "gen " + entry.name + "(" + genMatch[2] + ")";
            entry.line = (int)(i + 1);
            if (!pendingComments.empty()) {
                for (auto& c : pendingComments) entry.docComment += c + " ";
                pendingComments.clear();
            }
            entries.push_back(entry);
            continue;
        }

        // Check for const/let variable declarations
        std::regex varRegex(R"(^(?:pub\s+)?(const|let)\s+(\w+))");
        std::smatch varMatch;
        if (std::regex_search(trimmed, varMatch, varRegex)) {
            DocEntry entry;
            entry.type = "variable";
            entry.name = varMatch[2];
            entry.signature = varMatch[1] + " " + entry.name;
            entry.line = (int)(i + 1);
            if (!pendingComments.empty()) {
                for (auto& c : pendingComments) entry.docComment += c + " ";
                pendingComments.clear();
            }
            entries.push_back(entry);
            continue;
        }

        // Non-declaration line clears pending comments
        pendingComments.clear();
    }

    return entries;
}

std::string generateMarkdown(const std::string& filename, const std::vector<DocEntry>& entries) {
    std::ostringstream md;
    md << "# Documentation: " << filename << "\n\n";

    // Group entries by type
    std::vector<DocEntry*> functions, classes, methods, generators, variables;
    for (auto& e : entries) {
        switch (e.type[0]) {
            case 'f': functions.push_back(const_cast<DocEntry*>(&e)); break;
            case 'c': classes.push_back(const_cast<DocEntry*>(&e)); break;
            case 'm': methods.push_back(const_cast<DocEntry*>(&e)); break;
            case 'g': generators.push_back(const_cast<DocEntry*>(&e)); break;
            case 'v': variables.push_back(const_cast<DocEntry*>(&e)); break;
        }
    }

    if (!functions.empty()) {
        md << "## Functions\n\n";
        for (auto* f : functions) {
            md << "### `" << f->signature << "`\n";
            md << "**Line:** " << f->line << "\n\n";
            if (!f->docComment.empty()) md << f->docComment << "\n\n";
        }
    }

    if (!generators.empty()) {
        md << "## Generators\n\n";
        for (auto* g : generators) {
            md << "### `" << g->signature << "`\n";
            md << "**Line:** " << g->line << "\n\n";
            if (!g->docComment.empty()) md << g->docComment << "\n\n";
        }
    }

    if (!classes.empty()) {
        md << "## Classes\n\n";
        for (auto* c : classes) {
            md << "### `" << c->signature << "`\n";
            md << "**Line:** " << c->line << "\n\n";
            if (!c->docComment.empty()) md << c->docComment << "\n\n";

            // Find methods belonging to this class
            for (auto* m : methods) {
                md << "- `" << m->signature << "` (line " << m->line << ")";
                if (!m->docComment.empty()) md << " - " << m->docComment;
                md << "\n";
            }
            md << "\n";
        }
    }

    if (!variables.empty()) {
        md << "## Variables\n\n";
        for (auto* v : variables) {
            md << "- **" << v->name << "** (`" << v->signature << "`) - Line " << v->line;
            if (!v->docComment.empty()) md << " - " << v->docComment;
            md << "\n";
        }
        md << "\n";
    }

    return md.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: docgen <file.fge> [output.md]\n";
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath;
    if (argc >= 3) {
        outputPath = argv[2];
    } else {
        outputPath = inputPath.substr(0, inputPath.rfind('.')) + ".md";
    }

    std::ifstream file(inputPath);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << inputPath << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    auto entries = extractDocs(source);
    std::string markdown = generateMarkdown(inputPath, entries);

    std::ofstream out(outputPath);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot write to: " << outputPath << "\n";
        return 1;
    }
    out << markdown;
    out.close();

    std::cout << "Generated documentation: " << outputPath << "\n";
    std::cout << "Found " << entries.size() << " declarations.\n";
    return 0;
}
