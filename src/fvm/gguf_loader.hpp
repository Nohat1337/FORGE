#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <memory>

namespace forge {
namespace fvm {

// ============================================================================
// GGUF File Format
// ============================================================================

enum class GGUFType : uint32_t {
    UINT8 = 0, INT8 = 1, UINT16 = 2, INT16 = 3,
    UINT32 = 4, INT32 = 5, FLOAT32 = 6, BOOL = 7,
    STRING = 8, ARRAY = 9, UINT64 = 10, INT64 = 11,
    FLOAT64 = 12
};

enum class GGMLType : uint32_t {
    F32 = 0, F16 = 1, Q4_0 = 2, Q4_1 = 3,
    Q5_0 = 6, Q5_1 = 7, Q8_0 = 8, Q8_1 = 9,
    Q2_K = 10, Q3_K = 11, Q4_K = 12, Q5_K = 13,
    Q6_K = 14, COUNT = 23, UNK = 255
};

struct GGUFValue {
    GGUFType type = GGUFType::UINT8;
    uint64_t u64_val = 0;
    double f64_val = 0;
    std::string s;
    std::vector<GGUFValue> arr;
};

struct GGUFTensorInfo {
    std::string name;
    std::vector<uint64_t> dimensions;
    GGMLType type = GGMLType::F32;
    uint64_t offset = 0;
};

struct GGUFHeader {
    uint32_t version = 0;
    uint64_t nTensors = 0;
    uint64_t nKV = 0;
    std::unordered_map<std::string, GGUFValue> metadata;
    std::vector<GGUFTensorInfo> tensors;
    uint64_t dataOffset = 0;
};

// ============================================================================
// Dequantization
// ============================================================================

constexpr int QK4_0 = 32;
constexpr int QK8_0 = 32;
constexpr int QK_K = 256;

int ggml_dequantize_row(const uint8_t* src, float* dst, int64_t n, GGMLType type);
int64_t ggml_row_size(int64_t ne, GGMLType type);

// ============================================================================
// LLaMA Tensor (holds weight data with lazy dequantization)
// ============================================================================

struct LlamaTensor {
    std::string name;
    GGMLType ggmlType = GGMLType::F32;
    std::vector<int64_t> shape;
    mutable std::vector<float> data_f32;
    mutable std::vector<uint8_t> data_raw;
    bool loaded = false;

    int64_t numel() const {
        int64_t n = 1;
        for (auto d : shape) n *= d;
        return n;
    }

    const float* as_float() const;
};

// ============================================================================
// LLaMA Layer
// ============================================================================

struct LlamaLayer {
    LlamaTensor qProj, kProj, vProj, oProj;
    LlamaTensor gateProj, upProj, downProj;
    LlamaTensor attnNorm, ffnNorm;
};

// ============================================================================
// LoRA Adapter
// ============================================================================

struct LlamaLoRA {
    int rank = 0;
    float alpha = 16.0f;
    float scale = 1.0f;

    struct Adapter {
        std::vector<float> A;  // [inDim, rank]
        std::vector<float> B;  // [rank, outDim]
        int inDim = 0, outDim = 0;
        void init(int in, int out, int r, float dropout);
        void apply(const float* input, float* output, int inD, int outD, float sc) const;
        void mergeInto(std::vector<float>& weight, int inD, int outD, float sc);
    };

    Adapter qA, qB;
    Adapter kA, kB;
    Adapter vA, vB;
    Adapter oA, oB;
    Adapter gateA, gateB;
    Adapter upA, upB;
    Adapter downA, downB;

    void init(int rank, float alpha, int dModel, int nKVHeads, int headDim, int dFF);
    void applyQ(const float* input, float* output, int dModel) const;
    void applyK(const float* input, float* output, int dModel) const;
    void applyV(const float* input, float* output, int dModel) const;
    void applyO(const float* input, float* output, int attnDim) const;
    void applyGate(const float* input, float* output, int dModel) const;
    void applyUp(const float* input, float* output, int dModel) const;
    void applyDown(const float* input, float* output, int dFF) const;

    void save(const std::string& path) const;
    bool load(const std::string& path);

    size_t paramCount() const;
};

// ============================================================================
// LLaMA KV Cache
// ============================================================================

struct LlamaKVCache {
    std::vector<float> keyCache;
    std::vector<float> valueCache;
    int capacity = 0;
    int pos = 0;
    int nLayers = 0;
    int nKVHeads = 0;
    int headDim = 0;

    void init(int nLayers, int nKVHeads, int headDim, int maxContext);
};

// ============================================================================
// LLaMA Model
// ============================================================================

struct LlamaConfig {
    int32_t nLayers = 0;
    int32_t nHeads = 0;
    int32_t nKVHeads = 0;
    int32_t headDim = 0;
    int32_t dModel = 0;
    int32_t vocabSize = 0;
    int32_t contextLength = 0;
    float ropeFreqBase = 10000.0f;
    float ropeScale = 1.0f;
    float normEps = 1e-5f;
    std::string archName;
    std::string modelName;
};

struct LlamaModel {
    LlamaConfig config;
    LlamaTensor tokenEmb;
    LlamaTensor outputNorm;
    LlamaTensor outputProj;
    std::vector<LlamaLayer> layers;
    LlamaKVCache kvCache;

    bool loaded = false;

    std::vector<float> logits;

    // LoRA
    std::vector<LlamaLoRA> loraLayers;
    bool loraEnabled = false;

    // Custom model name (user's fine-tuned model)
    std::string customName;

    bool loadFromGGUF(const std::string& path);

    std::vector<int32_t> tokenize(const std::string& text);
    std::string detokenize(const std::vector<int32_t>& tokens);

    std::vector<float> forward(int32_t token, int pos);
    std::vector<int32_t> generate(const std::vector<int32_t>& prompt, int maxTokens,
                                   float temperature = 0.8f, int topK = 40, float topP = 0.9f);

    int32_t sampleNext(const float* logits, float temperature, int topK, float topP);

    // LoRA fine-tuning
    void enableLoRA(int rank = 16, float alpha = 16.0f);
    void disableLoRA();
    float trainStep(const std::vector<int32_t>& tokens, float lr = 1e-4f);
    void saveLoRA(const std::string& path);
    void loadLoRA(const std::string& path);
    void mergeLoRA();

    // Real embeddings from token_embd matrix
    std::vector<float> embed(const std::string& text);
    float similarity(const std::string& a, const std::string& b);

    // Model rename (for user's custom model)
    void setCustomName(const std::string& name);

private:
    std::vector<float> rmsNorm(const float* x, const LlamaTensor& weight, int dim);
    std::vector<float> attention(int layer, const float* x, int pos);
    std::vector<float> ffn(int layer, const float* x);
    void matmul(const float* input, const LlamaTensor& weight, float* output, int inDim, int outDim);

    // LoRA-augmented forward
    std::vector<float> attentionLoRA(int layer, const float* x, int pos);
    std::vector<float> ffnLoRA(int layer, const float* x);

    // Gradient storage for LoRA params
    std::vector<std::vector<float>> loraGradsA, loraGradsB;

    std::vector<std::string> vocabTokens;
    std::unordered_map<std::string, int32_t> vocabMap;
    int32_t bosToken = 1, eosToken = 2;
    bool hasTokenizer = false;

    void loadTokenizer(const GGUFHeader& header);
};

extern LlamaModel g_llama;

} // namespace fvm
} // namespace forge
