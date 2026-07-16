#include "faw.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <fstream>

namespace forge {
namespace fvm {

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
    // Tool: web_search
    registerTool({"web_search", "Search the web for information",
        {"query"},
        [](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            auto it = args.find("query");
            if (it == args.end()) return {false, "", "Missing query parameter"};
            // Use curl to fetch search results
            std::string query = it->second;
            std::string cmd = "curl -s \"https://api.duckduckgo.com/?q=" + query + "&format=json&no_html=1\" 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return {false, "", "Failed to execute search"};
            char buffer[8192];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
            pclose(pipe);
            if (result.empty()) return {false, "", "No results found"};
            // Extract AbstractText from JSON (simple parse)
            size_t absPos = result.find("\"AbstractText\":\"");
            if (absPos != std::string::npos) {
                size_t start = absPos + 16;
                size_t end = result.find("\"", start);
                if (end != std::string::npos) {
                    std::string abstract = result.substr(start, end - start);
                    if (!abstract.empty()) return {true, abstract, ""};
                }
            }
            // Try RelatedTopics
            size_t relPos = result.find("\"Text\":\"");
            if (relPos != std::string::npos) {
                size_t start = relPos + 8;
                size_t end = result.find("\"", start);
                if (end != std::string::npos)
                    return {true, result.substr(start, end - start), ""};
            }
            return {true, "Search completed but no summary available. Raw: " + result.substr(0, 500), ""};
        }});

    // Tool: read_file
    registerTool({"read_file", "Read the contents of a file",
        {"path"},
        [](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            auto it = args.find("path");
            if (it == args.end()) return {false, "", "Missing path parameter"};
            std::ifstream f(it->second);
            if (!f.is_open()) return {false, "", "Cannot open file: " + it->second};
            std::string content((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
            if (content.size() > 4096) content = content.substr(0, 4096) + "\n... [truncated]";
            return {true, content, ""};
        }});

    // Tool: write_file
    registerTool({"write_file", "Write content to a file",
        {"path", "content"},
        [](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            auto p = args.find("path");
            auto c = args.find("content");
            if (p == args.end() || c == args.end()) return {false, "", "Missing path or content"};
            std::ofstream f(p->second);
            if (!f.is_open()) return {false, "", "Cannot write to file: " + p->second};
            f << c->second;
            return {true, "File written: " + p->second, ""};
        }});

    // Tool: execute_command
    registerTool({"execute_command", "Execute a shell command",
        {"command"},
        [](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            auto it = args.find("command");
            if (it == args.end()) return {false, "", "Missing command parameter"};
            std::string cmd = it->second + " 2>&1";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return {false, "", "Failed to execute command"};
            char buffer[4096];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe) && result.size() < 8192) result += buffer;
            pclose(pipe);
            return {true, result, ""};
        }});

    // Tool: get_time
    registerTool({"get_time", "Get the current date and time", {},
        [](const std::unordered_map<std::string, std::string>&) -> ToolResult {
            time_t now = time(nullptr);
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
            return {true, std::string(buf), ""};
        }});

    // Tool: calculate
    registerTool({"calculate", "Evaluate a mathematical expression", {"expression"},
        [](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            auto it = args.find("expression");
            if (it == args.end()) return {false, "", "Missing expression"};
            // Simple calculator: use python
            std::string cmd = "python3 -c \"print(" + it->second + ")\" 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return {false, "", "Calculator failed"};
            char buf[256];
            std::string result;
            if (fgets(buf, sizeof(buf), pipe)) result = buf;
            pclose(pipe);
            // Trim newline
            while (!result.empty() && result.back() == '\n') result.pop_back();
            return result.empty() ? ToolResult{false, "", "Calculation failed"} : ToolResult{true, result, ""};
        }});

    // Tool: list_files
    registerTool({"list_files", "List files in a directory", {"path"},
        [](const std::unordered_map<std::string, std::string>& args) -> ToolResult {
            auto it = args.find("path");
            std::string path = (it != args.end()) ? it->second : ".";
            std::string cmd = "ls -la " + path + " 2>/dev/null | head -30";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return {false, "", "Failed to list files"};
            char buffer[4096];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
            pclose(pipe);
            return {true, result, ""};
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
