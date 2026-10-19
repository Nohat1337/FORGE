#include "faw.hpp"
#include "runtime.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <fstream>
#include <filesystem>
#include <cctype>

namespace fs = std::filesystem;

namespace forge {
namespace fvm {

// ============================================================
// Safe math expression evaluator (replaces `python3 -c` shell
// execution, which allowed arbitrary code injection through a
// crafted model tool-call). Grammar:
//   expr   := term (('+'|'-') term)*
//   term   := unary (('*'|'/'|'%') unary)*
//   unary  := ('-'|'+')? power
//   power  := atom ('^' unary)?
//   atom   := NUMBER | '(' expr ')' | IDENT '(' expr (',' expr)* ')'
// Supported functions: sqrt, abs, sin, cos, tan, log, floor, ceil, round, min, max, pow
// ============================================================

class SafeCalc {
public:
    static std::string eval(const std::string& src) {
        SafeCalc c(src);
        c.skipWs();
        double v = c.parseExpr();
        c.skipWs();
        if (c.pos_ < c.s_.size())
            throw std::runtime_error("calculate: unexpected character '" +
                                     std::string(1, c.s_[c.pos_]) + "'");
        // pretty-print result (integers without decimals)
        if (std::fabs(v) < 1e15 && v == std::floor(v))
            return std::to_string((long long)v);
        std::ostringstream o;
        o << v;
        return o.str();
    }

private:
    explicit SafeCalc(const std::string& s) : s_(s) {}
    std::string s_;
    size_t pos_ = 0;
    int depth_ = 0;

    void skipWs() { while (pos_ < s_.size() && isspace((unsigned char)s_[pos_])) pos_++; }
    char peek() { return pos_ < s_.size() ? s_[pos_] : '\0'; }

    double parseExpr() {
        double v = parseTerm();
        for (;;) {
            skipWs();
            if (peek() == '+') { pos_++; v += parseTerm(); }
            else if (peek() == '-') { pos_++; v -= parseTerm(); }
            else return v;
        }
    }
    double parseTerm() {
        double v = parseUnary();
        for (;;) {
            skipWs();
            if (peek() == '*') { pos_++; v *= parseUnary(); }
            else if (peek() == '/') {
                pos_++;
                double d = parseUnary();
                if (d == 0) throw std::runtime_error("calculate: division by zero");
                v /= d;
            }
            else if (peek() == '%') {
                pos_++;
                double d = parseUnary();
                if (d == 0) throw std::runtime_error("calculate: division by zero");
                v = std::fmod(v, d);
            }
            else return v;
        }
    }
    double parseUnary() {
        skipWs();
        if (peek() == '-') { pos_++; return -parseUnary(); }
        if (peek() == '+') { pos_++; return parseUnary(); }
        return parsePower();
    }
    double parsePower() {
        double base = parseAtom();
        skipWs();
        if (peek() == '^') { pos_++; return std::pow(base, parseUnary()); } // right-assoc
        return base;
    }
    double parseAtom() {
        skipWs();
        if (depth_ > 32) throw std::runtime_error("calculate: expression too deep");
        if (peek() == '(') {
            pos_++;
            depth_++;
            double v = parseExpr();
            depth_--;
            skipWs();
            if (peek() != ')') throw std::runtime_error("calculate: missing ')'");
            pos_++;
            return v;
        }
        if (isalpha((unsigned char)peek())) {
            std::string name;
            while (isalpha((unsigned char)peek()) || peek() == '_') name += s_[pos_++];
            skipWs();
            if (peek() != '(') throw std::runtime_error("calculate: unknown identifier '" + name + "'");
            pos_++;
            depth_++;
            std::vector<double> args;
            skipWs();
            if (peek() != ')') {
                args.push_back(parseExpr());
                skipWs();
                while (peek() == ',') { pos_++; args.push_back(parseExpr()); skipWs(); }
            }
            depth_--;
            if (peek() != ')') throw std::runtime_error("calculate: missing ')'");
            pos_++;
            return applyFn(name, args);
        }
        // number literal (no exec of any kind)
        size_t start = pos_;
        while (isdigit((unsigned char)peek()) || peek() == '.') pos_++;
        if (peek() == 'e' || peek() == 'E') {
            size_t save = pos_;
            pos_++;
            if (peek() == '+' || peek() == '-') pos_++;
            if (isdigit((unsigned char)peek())) { while (isdigit((unsigned char)peek())) pos_++; }
            else pos_ = save;
        }
        if (start == pos_) throw std::runtime_error("calculate: expected number");
        try { return std::stod(s_.substr(start, pos_ - start)); }
        catch (...) { throw std::runtime_error("calculate: invalid number"); }
    }

    static double applyFn(const std::string& n, const std::vector<double>& a) {
        auto need = [&](size_t k) {
            if (a.size() != k) throw std::runtime_error("calculate: " + n + "() expects " +
                                 std::to_string(k) + " argument(s)");
        };
        if (n == "sqrt")  { need(1); if (a[0] < 0) throw std::runtime_error("calculate: sqrt of negative"); return std::sqrt(a[0]); }
        if (n == "abs")   { need(1); return std::fabs(a[0]); }
        if (n == "sin")   { need(1); return std::sin(a[0]); }
        if (n == "cos")   { need(1); return std::cos(a[0]); }
        if (n == "tan")   { need(1); return std::tan(a[0]); }
        if (n == "log")   { need(1); if (a[0] <= 0) throw std::runtime_error("calculate: log domain"); return std::log(a[0]); }
        if (n == "floor") { need(1); return std::floor(a[0]); }
        if (n == "ceil")  { need(1); return std::ceil(a[0]); }
        if (n == "round") { need(1); return std::round(a[0]); }
        if (n == "pow")   { need(2); return std::pow(a[0], a[1]); }
        if (n == "min")   { need(2); return std::min(a[0], a[1]); }
        if (n == "max")   { need(2); return std::max(a[0], a[1]); }
        throw std::runtime_error("calculate: unknown function '" + n + "'");
    }
};

// percent-encode for safe URL interpolation
static std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
        else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

// Resolve path and ensure it stays inside the workspace when sandboxed.
static bool pathAllowed(const std::string& p) {
    if (g_globalTrustedMode) return true;   // trusted mode lifts the sandbox
    try {
        fs::path abs = fs::absolute(p).lexically_normal();
        fs::path cwd = fs::current_path();
        auto rel = abs.lexically_relative(cwd);
        if (rel.empty() || *rel.begin() == "..") return false;
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// Tool Call Parser
// ============================================================================

std::vector<ForgeAgent::ToolCall> ForgeAgent::parseToolCalls(const std::string& text) {
    std::vector<ToolCall> calls;
    // Parse: <tool_call>{"name":"tool_name","args":{"key":"value"}}</tool_call>
    std::regex re("<tool_call>\\s*\\{(.*?)\\}\\s*</tool_call>", std::regex::ECMAScript);
    auto begin = std::sregex_iterator(text.begin(), text.end(), re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string json = (*it)[1].str();
        ToolCall tc;
        std::regex nameRe("\"name\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch nm;
        if (std::regex_search(json, nm, nameRe)) tc.name = nm[1].str();
        std::regex argRe("\"(\\w+)\"\\s*:\\s*\"([^\"]*)\"");
        auto argBegin = std::sregex_iterator(json.begin(), json.end(), argRe);
        auto argEnd = std::sregex_iterator();
        for (auto ait = argBegin; ait != argEnd; ++ait) {
            std::string key = (*ait)[1].str();
            if (key != "name") tc.arguments[key] = (*ait)[2].str();
        }
        if (!tc.name.empty()) calls.push_back(tc);
    }
    return calls;
}

bool ForgeAgent::hasToolCall(const std::string& text) {
    return text.find("<tool_call>") != std::string::npos;
}

// ============================================================================
// ForgeAgent
// ============================================================================

ForgeAgent g_agent;

ForgeAgent::ForgeAgent() {}

void ForgeAgent::init(LlamaModel* model, const AgentConfig& config) {
    model_ = model;
    config_ = config;
    registerBuiltinTools();
    fprintf(stderr, "[FAW] Agent initialized with %lu tools\n", (unsigned long)tools_.size());
}

void ForgeAgent::registerTool(const Tool& tool) {
    tools_[tool.name] = tool;
    fprintf(stderr, "[FAW] Tool registered: %s\n", tool.name.c_str());
}

void ForgeAgent::registerBuiltinTools() {
    // Tool: web_search  (query is percent-encoded -> no shell injection)
    registerTool({"web_search", "Search the web for information",
        {"query"},
        [this](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            auto it = args.find("query");
            if (it == args.end()) return {false, "", "Missing query parameter"};
            std::string cmd = "curl -s --max-time 10 \"https://api.duckduckgo.com/?q="
                              + urlEncode(it->second) + "&format=json&no_html=1\" 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return {false, "", "Failed to execute search"};
            char buffer[8192];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
            pclose(pipe);
            if (result.empty()) return {false, "", "No results found"};
            size_t absPos = result.find("\"AbstractText\":\"");
            if (absPos != std::string::npos) {
                size_t start = absPos + 16;
                size_t end = result.find("\"", start);
                if (end != std::string::npos) {
                    std::string abstract = result.substr(start, end - start);
                    if (!abstract.empty()) return {true, abstract, ""};
                }
            }
            size_t relPos = result.find("\"Text\":\"");
            if (relPos != std::string::npos) {
                size_t start = relPos + 8;
                size_t end = result.find("\"", start);
                if (end != std::string::npos)
                    return {true, result.substr(start, end - start), ""};
            }
            return {true, "Search completed but no summary available.", ""};
        }});

    // Tool: read_file (sandboxed to the workspace unless trusted)
    registerTool({"read_file", "Read the contents of a file inside the workspace",
        {"path"},
        [this](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            auto it = args.find("path");
            if (it == args.end()) return {false, "", "Missing path parameter"};
            if (!pathAllowed(it->second))
                return {false, "", "Sandbox: path outside workspace denied (" + it->second + ")"};
            std::ifstream f(it->second);
            if (!f.is_open()) return {false, "", "Cannot open file: " + it->second};
            std::string content((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
            if (content.size() > 4096) content = content.substr(0, 4096) + "\n... [truncated]";
            return {true, content, ""};
        }});

    // Tool: write_file (sandboxed; refuses to overwrite critical dotfiles)
    registerTool({"write_file", "Write content to a file inside the workspace",
        {"path", "content"},
        [this](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            auto p = args.find("path");
            auto c = args.find("content");
            if (p == args.end() || c == args.end()) return {false, "", "Missing path or content"};
            if (!pathAllowed(p->second))
                return {false, "", "Sandbox: path outside workspace denied (" + p->second + ")"};
            fs::path target = fs::absolute(p->second).lexically_normal();
            // never allow writing hidden/system files (shell startup files,
            // .ssh keys, .gitconfig, ...) even inside the sandbox
            if (!target.filename().string().compare(0, 1, "."))
                return {false, "", "Sandbox: refusing to write hidden/system file"};
            std::ofstream f(target, std::ios::binary | std::ios::trunc);
            if (!f.is_open()) return {false, "", "Cannot write to file: " + p->second};
            f << c->second;
            return {true, "File written: " + p->second, ""};
        }});

    // Tool: execute_command — DISABLED unless trusted mode.
    registerTool({"execute_command", "Execute a shell command (trusted mode only)",
        {"command"},
        [](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            if (!g_globalTrustedMode)
                return {false, "",
                        "execute_command is disabled in sandboxed mode — a language model "
                        "must never run arbitrary shell commands without explicit opt-in. "
                        "Run forgevm with --trusted to enable (only with models you trust)."};
            auto it = args.find("command");
            if (it == args.end()) return {false, "", "Missing command parameter"};
            std::string cmd = it->second + " 2>&1";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return {false, "", "Failed to execute command"};
            char buffer[4096];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe) && result.size() < 8192) result += buffer;
            int rc = pclose(pipe);
            return {true, result + "\n[exit code: " + std::to_string(rc >> 8) + "]", ""};
        }});

    // Tool: get_time
    registerTool({"get_time", "Get the current date and time", {},
        [](const std::unordered_map<std::string, std::string>&) -> ToolResult {
            time_t now = time(nullptr);
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
            return {true, std::string(buf), ""};
        }});

    // Tool: calculate — pure in-process evaluator (no shell, no python)
    registerTool({"calculate", "Evaluate a mathematical expression",
        {"expression"},
        [](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            auto it = args.find("expression");
            if (it == args.end()) return {false, "", "Missing expression"};
            try {
                return {true, SafeCalc::eval(it->second), ""};
            } catch (const std::exception& e) {
                return {false, "", e.what()};
            }
        }});

    // Tool: list_files — real filesystem API (no `ls` shell injection)
    registerTool({"list_files", "List files in a directory inside the workspace",
        {"path"},
        [this](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            auto it = args.find("path");
            std::string path = (it != args.end()) ? it->second : ".";
            if (!pathAllowed(path))
                return {false, "", "Sandbox: path outside workspace denied (" + path + ")"};
            std::error_code ec;
            fs::path dir = fs::canonical(fs::absolute(path), ec);
            if (ec) dir = fs::absolute(path).lexically_normal();
            if (!fs::is_directory(dir, ec)) return {false, "", "Not a directory: " + path};
            struct Entry { std::string name; bool isDir; uintmax_t size; };
            std::vector<Entry> entries;
            for (auto& e : fs::directory_iterator(dir, ec)) {
                entries.push_back({e.path().filename().string(),
                                   e.is_directory(ec),
                                   e.is_regular_file(ec) ? e.file_size(ec) : 0});
                if (entries.size() >= 100) break;
            }
            std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
                if (a.isDir != b.isDir) return a.isDir > b.isDir;
                return a.name < b.name;
            });
            std::ostringstream out;
            out << dir.string() << " (" << entries.size() << " entries)\n";
            size_t shown = 0;
            for (auto& e : entries) {
                if (shown++ >= 30) { out << "... and " << (entries.size() - 30) << " more\n"; break; }
                out << (e.isDir ? "[DIR]  " : "       ") << e.name;
                if (!e.isDir) out << "  (" << e.size << " bytes)";
                out << "\n";
            }
            return {true, out.str(), ""};
        }});

    // Tool: memory (conversation context)
    registerTool({"memory", "Store or retrieve information from agent memory",
        {"action", "key", "value"},
        [](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            static std::unordered_map<std::string, std::string> store;
            auto act = args.find("action");
            auto key = args.find("key");
            if (act == args.end() || key == args.end())
                return {false, "", "Missing action or key"};
            if (act->second == "set") {
                auto val = args.find("value");
                store[key->second] = (val != args.end()) ? val->second : "";
                return {true, "Stored: " + key->second, ""};
            } else if (act->second == "get") {
                auto it = store.find(key->second);
                return it != store.end() ? ToolResult{true, it->second, ""}
                                         : ToolResult{false, "", "Key not found"};
            } else if (act->second == "list") {
                std::string keys;
                for (auto& [k, v] : store) keys += k + ", ";
                return {true, keys.empty() ? "(empty)" : keys, ""};
            }
            return {false, "", "Unknown action"};
        }});
}

std::string ForgeAgent::invokeTool(const std::string& name,
                                   const std::unordered_map<std::string, std::string>& args) {
    auto it = tools_.find(name);
    if (it == tools_.end()) return "Error: tool '" + name + "' not found";
    ToolResult tr = it->second.execute(args);
    return tr.success ? tr.output : "Error: " + tr.error;
}

std::string ForgeAgent::getToolDescriptions() const {
    std::string desc;
    for (auto& [name, tool] : tools_) {
        desc += "- " + name + ": " + tool.description + "\n";
        if (!tool.parameters.empty()) {
            desc += "  Parameters: ";
            for (auto& p : tool.parameters) desc += p + " ";
            desc += "\n";
        }
    }
    return desc;
}

std::string ForgeAgent::buildPrompt() {
    std::string prompt;
    if (!config_.systemPrompt.empty()) {
        prompt = "System: " + config_.systemPrompt + "\n\n";
    } else {
        prompt = "System: You are a helpful AI assistant. You have access to tools.\n"
                 "When you need to use a tool, output: <tool_call>{\"name\":\"tool_name\",\"args\":{\"param\":\"value\"}}</tool_call>\n"
                 "You can use multiple tools. After receiving tool results, continue helping the user.\n\n";
    }

    prompt += "Available tools:\n" + getToolDescriptions() + "\n";

    for (auto& msg : history_) {
        if (msg.role == "user") prompt += "User: " + msg.content + "\n";
        else if (msg.role == "assistant") prompt += "Assistant: " + msg.content + "\n";
        else if (msg.role == "tool") prompt += "Tool Result [" + msg.toolName + "]: " + msg.content + "\n";
    }

    prompt += "Assistant: ";
    return prompt;
}

std::string ForgeAgent::executeToolCalls(const std::string& text) {
    auto calls = parseToolCalls(text);
    std::string results;
    for (auto& tc : calls) {
        auto it = tools_.find(tc.name);
        if (it == tools_.end()) {
            results += "Tool '" + tc.name + "' not found.\n";
            history_.push_back({"tool", "Tool '" + tc.name + "' not found", tc.name, ""});
            continue;
        }
        ToolResult tr = it->second.execute(tc.arguments);
        std::string resultStr = tr.success ? tr.output : "Error: " + tr.error;
        results += "[" + tc.name + "]: " + resultStr + "\n";
        history_.push_back({"tool", resultStr, tc.name, ""});
        fprintf(stderr, "[FAW] Tool %s: %s\n", tc.name.c_str(), tr.success ? "OK" : "FAIL");
    }
    return results;
}

std::string ForgeAgent::step(const std::string& userMessage) {
    if (!model_ || !model_->loaded) return "[Error: No model loaded]";

    history_.push_back({"user", userMessage, "", ""});

    // Generate response
    std::string prompt = buildPrompt();
    auto tokens = model_->tokenize(prompt);
    auto generated = model_->generate(tokens, config_.maxTokensPerStep,
                                       config_.temperature, config_.topK, config_.topP);
    std::string response = model_->detokenize(generated);

    // Strip the prompt prefix from response
    size_t lastAssistant = response.rfind("Assistant: ");
    if (lastAssistant != std::string::npos)
        response = response.substr(lastAssistant + 11);

    history_.push_back({"assistant", response, "", ""});

    // Check for tool calls and execute them
    if (hasToolCall(response)) {
        std::string toolResults = executeToolCalls(response);
        if (!toolResults.empty()) {
            // Feed tool results back for a follow-up response
            history_.push_back({"user", "Here are the tool results:\n" + toolResults + "\nPlease summarize and help the user.", "", ""});
            prompt = buildPrompt();
            tokens = model_->tokenize(prompt);
            generated = model_->generate(tokens, config_.maxTokensPerStep,
                                         config_.temperature, config_.topK, config_.topP);
            response = model_->detokenize(generated);
            lastAssistant = response.rfind("Assistant: ");
            if (lastAssistant != std::string::npos)
                response = response.substr(lastAssistant + 11);
            history_.push_back({"assistant", response, "", ""});
        }
    }

    return response;
}

std::string ForgeAgent::run(const std::string& userMessage) {
    clearHistory();
    std::string response;
    for (int i = 0; i < config_.maxIterations; i++) {
        response = step(userMessage);
        // Check if agent wants to continue (has more tool calls)
        if (!hasToolCall(response)) break;
    }
    return response;
}

} // namespace fvm
} // namespace forge
