#pragma once

#include "llm.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace forge::llm {

// HF model converter - converts HuggingFace safetensors/pytorch models to Forge format
class HFModelConverter {
public:
    struct HFModelInfo {
        std::string architecture;  // "llama", "gpt2", "gptj", "mistral", etc.
        int vocab_size = 0;
        int hidden_size = 0;
        int num_layers = 0;
        int num_heads = 0;
        int num_kv_heads = 0;
        int intermediate_size = 0;
        int max_position_embeddings = 0;
        float rms_norm_eps = 1e-5f;
        bool tie_word_embeddings = true;
        std::string torch_dtype = "float16";
    };

    struct WeightMap {
        std::unordered_map<std::string, std::vector<float>> weights;
        std::unordered_map<std::string, std::vector<int64_t>> shapes;
    };

    // Load HF model from safetensors format
    static bool loadFromSafeTensors(const std::string& path, WeightMap& weights, HFModelInfo& info);

    // Load HF model from PyTorch .bin format
    static bool loadFromPyTorchBin(const std::string& path, WeightMap& weights, HFModelInfo& info);

    // Convert HF weights to Forge model
    static bool convertToForge(const WeightMap& hf_weights, const HFModelInfo& hf_info,
                                TransformerModel& forge_model);

    // Save Forge model to binary format
    static bool saveForgeModel(const TransformerModel& model, const Tokenizer& tokenizer,
                                const std::string& model_path, const std::string& tokenizer_path);

    // Load Forge model
    static bool loadForgeModel(const std::string& model_path, const std::string& tokenizer_path,
                                TransformerModel& model, Tokenizer& tokenizer, forge::llm::ModelConfig& config);

    // Detect model architecture from config.json
    static bool detectArchitecture(const std::string& config_path, HFModelInfo& info);

    // Supported architectures
    static std::vector<std::string> getSupportedArchitectures();

private:
    // Architecture-specific weight mapping
    static bool convertLlama(const WeightMap& hf, const HFModelInfo& info, TransformerModel& forge);
    static bool convertGPT2(const WeightMap& hf, const HFModelInfo& info, TransformerModel& forge);
    static bool convertMistral(const WeightMap& hf, const HFModelInfo& info, TransformerModel& forge);
    static bool convertGPTJ(const WeightMap& hf, const HFModelInfo& info, TransformerModel& forge);

    // Helper: remap weight names from HF to Forge format
    static std::string remapWeightName(const std::string& hf_name, const std::string& architecture);

    // Helper: transpose weight if needed
    static void maybeTranspose(std::vector<float>& weight, const std::vector<int64_t>& shape, 
                               bool transpose);

    // Helper: convert dtype (fp32/fp16/bf16)
    static void convertDtype(const void* src, float* dst, size_t count, const std::string& src_dtype);
};

} // namespace forge::llm