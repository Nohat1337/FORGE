#pragma once

#include "gguf_loader.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace forge {
namespace fvm {

// ============================================================================
// FAW - Forge Agent Wrap System
// ============================================================================

struct ToolResult {
    bool success = true;
    std::string output;
    std::string error;
};

struct Tool {
    std::string name;
    std::string description;
    std::vector<std::string> parameters;
    std::function<ToolResult(const std::unordered_map<std::string, std::string>&)> execute;
};

struct AgentMessage {
    std::string role;    // "system", "user", "assistant", "tool"
    std::string content;
    std::string toolName;
    std::string toolCallId;
};

struct AgentConfig {
    int maxIterations = 10;
    float temperature = 0.3f;
    int topK = 40;
    float topP = 0.9f;
    int maxTokensPerStep = 512;
    std::string systemPrompt;
};

class ForgeAgent {
public:
    ForgeAgent();
    ~ForgeAgent() = default;

    // Initialize with a loaded LLaMA model
    void init(LlamaModel* model, const AgentConfig& config = AgentConfig());

    // Register tools
    void registerTool(const Tool& tool);
    void registerBuiltinTools();

    // Run the agent loop
    std::string run(const std::string& userMessage);

    // Single step (for interactive use)
    std::string step(const std::string& userMessage);

    // Conversation history
    const std::vector<AgentMessage>& getHistory() const { return history_; }
    void clearHistory() { history_.clear(); }

    // Tool call parsing
    struct ToolCall {
        std::string name;
        std::unordered_map<std::string, std::string> arguments;
    };
    static std::vector<ToolCall> parseToolCalls(const std::string& text);

    // Check if text contains a tool call
    static bool hasToolCall(const std::string& text);

private:
    LlamaModel* model_ = nullptr;
    AgentConfig config_;
    std::unordered_map<std::string, Tool> tools_;
    std::vector<AgentMessage> history_;

    std::string buildPrompt();
    std::string executeToolCalls(const std::string& text);
    std::string getToolDescriptions() const;
};

extern ForgeAgent g_agent;

} // namespace fvm
} // namespace forge
