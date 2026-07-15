#pragma once

#include "runtime.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <random>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <optional>
#include <filesystem>
#include <numeric>
#include <execution>

namespace forge::llm {

// ============================================================================
// DATA PREPARATION
// ============================================================================

struct Dataset {
    std::vector<std::string> documents;
    std::vector<std::vector<int>> tokenized;
    std::unordered_map<std::string, int> metadata;
    
    void addDocument(const std::string& text, const std::string& source = "") {
        documents.push_back(text);
        if (!source.empty()) metadata[source] = documents.size() - 1;
    }
    
    void addDocuments(const std::vector<std::string>& texts) {
        for (const auto& t : texts) addDocument(t);
    }
    
    static Dataset fromDirectory(const std::string& path, const std::vector<std::string>& extensions = {".txt", ".md", ".json", ".cpp", ".h", ".hpp", ".py", ".js", ".ts"});
    static Dataset fromJsonl(const std::string& path);
    static Dataset fromFile(const std::string& path);
    
    std::tuple<Dataset, Dataset, Dataset> split(float trainRatio = 0.9f, float valRatio = 0.05f);
    
    void shuffle(uint32_t seed = 42);
    
    void filterByLength(size_t minLen = 10, size_t maxLen = 1000000);
    
    void deduplicate(float threshold = 0.9f);
    
    void save(const std::string& path) const;
    static Dataset load(const std::string& path);
    
    size_t size() const { return documents.size(); }
    size_t totalChars() const {
        size_t sum = 0;
        for (const auto& d : documents) sum += d.size();
        return sum;
    }
};

// ============================================================================
// TOKENIZER (Enhanced BPE) - Must be before DataLoader
// ============================================================================

class Tokenizer {
public:
    Tokenizer() = default;
    
    // Train BPE tokenizer from text corpus
    void train(const std::string& text, int vocabSize = 32000, int minFrequency = 2);
    void trainFromDataset(const Dataset& dataset, int vocabSize = 32000, int minFrequency = 2);
    
    // Encode/decode
    std::vector<int> encode(const std::string& text, bool addBos = true, bool addEos = true) const;
    std::string decode(const std::vector<int>& tokens, bool skipSpecial = true) const;
    
    // Special tokens
    int bosTokenId() const { return bosId_; }
    int eosTokenId() const { return eosId_; }
    int padTokenId() const { return padId_; }
    int unkTokenId() const { return unkId_; }
    
    void setSpecialTokens(const std::string& bos, const std::string& eos, const std::string& pad, const std::string& unk);
    
    int vocabSize() const { return vocab_.size(); }
    int tokenToId(const std::string& token) const {
        auto it = vocab_.find(token);
        return it != vocab_.end() ? it->second : unkId_;
    }
    std::string idToToken(int id) const {
        auto it = idToToken_.find(id);
        return it != idToToken_.end() ? it->second : "";
    }
    
    // Save/load
    void save(const std::string& path) const;
    void load(const std::string& path);
    
    // Get vocab for inspection
    const std::unordered_map<std::string, int>& getVocab() const { return vocab_; }
    const std::unordered_map<std::string, int>& getMerges() const { return merges_; }
    
private:
    std::vector<int> encodeWord(const std::string& word) const;
    std::string escape(const std::string& s) const;
    std::string unescape(const std::string& s) const;
    std::string toHex(uint8_t c) const;
    
    std::unordered_map<std::string, int> vocab_;
    std::unordered_map<int, std::string> idToToken_;
    std::unordered_map<std::string, int> merges_;
    
    int bosId_ = 1;
    int eosId_ = 2;
    int padId_ = 3;
    int unkId_ = 0;
};

struct DataLoader {
    Dataset* dataset = nullptr;
    std::shared_ptr<Tokenizer> tokenizer;
    int batchSize = 32;
    int maxSeqLen = 512;
    bool shuffle = true;
    size_t currentIdx = 0;
    std::vector<size_t> indices;
    
    DataLoader() = default;
    DataLoader(Dataset* ds, std::shared_ptr<Tokenizer> tok, int bs = 32, int ms = 512, bool sh = true)
        : dataset(ds), tokenizer(tok), batchSize(bs), maxSeqLen(ms), shuffle(sh) {
        reset();
    }
    
    void reset() {
        indices.resize(dataset->documents.size());
        std::iota(indices.begin(), indices.end(), 0);
        if (shuffle) {
            std::mt19937 rng(42);
            std::shuffle(indices.begin(), indices.end(), rng);
        }
        currentIdx = 0;
    }
    
    // Returns pair of (input_tokens, target_tokens) for next-token prediction
    std::pair<std::vector<std::vector<int>>, std::vector<std::vector<int>>> nextBatch() {
        std::vector<std::vector<int>> inputs, targets;
        for (int i = 0; i < batchSize && currentIdx < indices.size(); i++, currentIdx++) {
            auto tokens = tokenizer->encode(dataset->documents[indices[currentIdx]]);
            if (tokens.size() > maxSeqLen) tokens.resize(maxSeqLen);
            if (tokens.size() < 2) continue;
            
            // Input: all but last, Target: all but first
            inputs.push_back(std::vector<int>(tokens.begin(), tokens.end() - 1));
            targets.push_back(std::vector<int>(tokens.begin() + 1, tokens.end()));
        }
        return {inputs, targets};
    }
    
    bool hasNext() const { return currentIdx < indices.size(); }
    size_t batchesPerEpoch() const { return (indices.size() + batchSize - 1) / batchSize; }
};

// ============================================================================
// MODEL ARCHITECTURE - Transformer
// ============================================================================

struct ModelConfig {
    int vocabSize = 32000;
    int maxSeqLen = 2048;
    int dModel = 512;
    int numHeads = 8;
    int numLayers = 6;
    int dFF = 2048;
    float dropout = 0.1f;
    float layerNormEps = 1e-5f;
    bool useBias = true;
    int tieWeights = 1;  // Tie input/output embeddings
    
    // LoRA config
    int loraRank = 0;  // 0 = disabled
    int loraAlpha = 16;
    float loraDropout = 0.05f;
    std::vector<std::string> loraTargetModules = {"q_proj", "v_proj", "k_proj", "o_proj", "ff1", "ff2"};
};

struct Linear {
    std::vector<float> weight;
    std::vector<float> bias;
    int inFeatures, outFeatures;
    bool hasBias = true;
    
    // LoRA parameters
    std::vector<float> loraA, loraB;  // Low-rank adaptation
    int loraRank = 0;
    float loraScale = 1.0f;
    
    Linear() = default;
    Linear(int inF, int outF, bool useBias = true) : inFeatures(inF), outFeatures(outF), hasBias(useBias) {
        weight.resize(inF * outF);
        if (hasBias) bias.resize(outF);
    }
    
    void forward(const float* input, float* output) const;
    void forwardWithLoRA(const float* input, float* output) const;
    
    void initXavier(float scale = 0.02f);
    void initZero();
    
    // LoRA initialization
    void initLoRA(int rank, float alpha, float dropout);
    
    // Merge LoRA weights into main weights
    void mergeLoRA();
    
    // Save/load
    void save(std::ofstream& f) const;
    void load(std::ifstream& f);
};

struct LayerNorm {
    std::vector<float> weight, bias;
    int features;
    float eps = 1e-5f;
    
    LayerNorm() = default;
    LayerNorm(int f, float e = 1e-5f) : features(f), eps(e) {
        weight.resize(f, 1.0f);
        bias.resize(f, 0.0f);
    }
    
    void forward(const float* input, float* output) const;
    void save(std::ofstream& f) const;
    void load(std::ifstream& f);
};

struct MultiHeadAttention {
    Linear qProj, kProj, vProj, oProj;
    int numHeads, headDim;
    float scale;
    
    MultiHeadAttention() = default;
    MultiHeadAttention(int dModel, int numHeads_, int dFF, bool useBias = true);
    
    // Forward: input [seqLen, dModel] -> output [seqLen, dModel]
    void forward(const float* input, float* output, int seqLen, int dModel, const float* mask = nullptr) const;
    
    void initXavier(float scale = 0.02f);
    void initLoRA(int rank, float alpha, float dropout);
    void mergeLoRA();
    void save(std::ofstream& f) const;
    void load(std::ifstream& f);
};

struct FeedForward {
    Linear ff1, ff2;  // dModel -> dFF -> dModel
    int dFF = 0;
    float dropout = 0.1f;
    
    FeedForward() = default;
    FeedForward(int dModel, int dFF_, bool useBias = true);
    
    void forward(const float* input, float* output, int seqLen, int dModel) const;
    
    void initXavier(float scale = 0.02f);
    void initLoRA(int rank, float alpha, float dropout);
    void mergeLoRA();
    void save(std::ofstream& f) const;
    void load(std::ifstream& f);
};

struct TransformerBlock {
    LayerNorm ln1, ln2;
    MultiHeadAttention attn;
    FeedForward ff;
    float dropout = 0.1f;
    
    TransformerBlock() = default;
    TransformerBlock(const ModelConfig& cfg);
    
    void forward(const float* input, float* output, int seqLen, int dModel, const float* mask = nullptr) const;
    
    void initLoRA(int rank, float alpha, float dropout);
    void mergeLoRA();
    void save(std::ofstream& f) const;
    void load(std::ifstream& f);
};

class TransformerModel {
public:
    ModelConfig config;
    
    std::vector<float> tokenEmb;    // [vocabSize, dModel]
    std::vector<float> posEmb;      // [maxSeqLen, dModel]
    std::vector<TransformerBlock> layers;
    LayerNorm lnFinal;
    Linear head;  // Output projection (tied with tokenEmb if config.tieWeights)
    
    TransformerModel() = default;
    TransformerModel(const ModelConfig& cfg) : config(cfg) { initialize(); }
    
    void initialize();
    
    // Forward pass - returns logits [seqLen, vocabSize]
    std::vector<float> forward(const std::vector<int>& tokens);
    
    // Forward with pre-allocated buffers for training
    void forwardTrain(const int* tokens, int batchSize, int seqLen, 
                     float* logits, float* loss = nullptr, const int* targets = nullptr);
    
    // Generate tokens
    std::vector<int> generate(const std::vector<int>& prompt, int maxTokens, 
                             float temperature = 1.0f, int topK = 50, float topP = 0.9f);
    
    // Training step (single batch)
    float trainStep(const int* inputTokens, const int* targetTokens, int batchSize, int seqLen, float lr);
    
    // LoRA methods
    void enableLoRA(int rank, float alpha = 16, float dropout = 0.05f);
    void disableLoRA();
    void mergeLoRA();
    bool hasLoRA() const { return config.loraRank > 0; }
    
    // Save/load
    void save(const std::string& path) const;
    void load(const std::string& path);
    
    // Model info
    size_t parameterCount() const;
    size_t trainableParameterCount() const;
    std::string summary() const;
    
private:
    // Temp buffers (reused across forward passes)
    mutable std::vector<float> temp1_, temp2_, temp3_, temp4_;
    mutable std::vector<float> q_, k_, v_, attnOut_;
    mutable std::vector<float> logits_;
    mutable std::vector<float> residual_;
    
    void gelu(float* x, int n) const;
    void softmax(float* x, int n) const;
    void rope(float* q, float* k, int seqLen, int headDim) const;
};

// ============================================================================
// TRAINING INFRASTRUCTURE
// ============================================================================

struct TrainingConfig {
    int batchSize = 8;
    int maxSeqLen = 512;
    float learningRate = 3e-4f;
    float weightDecay = 0.01f;
    float beta1 = 0.9f;
    float beta2 = 0.95f;
    float eps = 1e-8f;
    int warmupSteps = 100;
    int maxSteps = 10000;
    int evalInterval = 500;
    int saveInterval = 1000;
    int logInterval = 10;
    int gradAccumSteps = 1;
    float maxGradNorm = 1.0f;
    std::string outputDir = "./checkpoints";
    bool useMixedPrecision = false;
    int numWorkers = 0;
    uint32_t seed = 42;
};

struct OptimizerState {
    std::vector<float> m, v;  // Adam moments
    int step = 0;
};

class Trainer {
public:
    Trainer(TransformerModel* model, const TrainingConfig& config = TrainingConfig());
    
    // Train on dataset
    void train(Dataset* trainData, Dataset* valData = nullptr, 
               std::shared_ptr<Tokenizer> tokenizer = nullptr);
    
    // Single training step
    float step(const std::vector<int>& inputTokens, const std::vector<int>& targetTokens);
    
    // Evaluation
    float evaluate(Dataset* valData, std::shared_ptr<Tokenizer> tokenizer, int numBatches = 10);
    
    // Save/load checkpoint
    void saveCheckpoint(const std::string& path) const;
    void loadCheckpoint(const std::string& path);
    
    // Learning rate schedule
    float getLr(int step) const;
    
    // Get training stats
    struct Stats {
        int step = 0;
        float trainLoss = 0;
        float valLoss = 0;
        float learningRate = 0;
        float tokensPerSec = 0;
    };
    Stats getStats() const { return stats_; }
    
    void setCallback(std::function<void(const Stats&)> cb) { callback_ = cb; }
    
private:
    TransformerModel* model_;
    TrainingConfig config_;
    OptimizerState optState_;
    Stats stats_;
    std::function<void(const Stats&)> callback_;
    std::mt19937 rng_;
    
    void updateParameters(const std::vector<float>& grads, float lr);
    void clipGradients(std::vector<float>& grads, float maxNorm);
};

// ============================================================================
// LORA FINE-TUNING
// ============================================================================

struct LoRAConfig {
    int rank = 16;
    int alpha = 16;
    float dropout = 0.05f;
    std::vector<std::string> targetModules = {"q_proj", "v_proj"};
    bool trainableEmbeddings = false;
};

class LoRATrainer {
public:
    LoRATrainer(TransformerModel* model, const LoRAConfig& config = LoRAConfig());
    
    // Apply LoRA to model
    void applyLoRA();
    
    // Fine-tune on dataset
    void fineTune(Dataset* trainData, std::shared_ptr<Tokenizer> tokenizer, 
                  const TrainingConfig& trainConfig = TrainingConfig());
    
    // Merge LoRA weights (for inference without LoRA overhead)
    void mergeAndSave(const std::string& path);
    
    // Save LoRA adapter only (small file)
    void saveAdapter(const std::string& path) const;
    
    // Load LoRA adapter
    void loadAdapter(const std::string& path);
    
private:
    TransformerModel* model_;
    LoRAConfig config_;
};

// ============================================================================
// MODEL MANAGER
// ============================================================================

class LLMManager {
public:
    static LLMManager& instance() {
        static LLMManager mgr;
        return mgr;
    }
    
    struct LoadedModel {
        std::string name;
        std::shared_ptr<TransformerModel> model;
        std::shared_ptr<Tokenizer> tokenizer;
        ModelConfig config;
        std::string modelPath;
        std::string tokenizerPath;
    };
    
    // Load model from files
    bool loadModel(const std::string& name, const std::string& modelPath, 
                   const std::string& tokenizerPath = "");
    
    // Create new model with config
    bool createModel(const std::string& name, const ModelConfig& config);
    
    // Get loaded model
    std::optional<LoadedModel> getModel(const std::string& name);
    
    // List loaded models
    std::vector<std::string> listModels() const;
    
    // Unload model
    void unloadModel(const std::string& name);
    
    // Generate text
    std::string generate(const std::string& modelName, const std::string& prompt, 
                        int maxTokens = 100, float temperature = 0.8f, 
                        int topK = 50, float topP = 0.9f);
    
    // Train/fine-tune
    void trainModel(const std::string& modelName, Dataset* trainData, 
                    Dataset* valData, const TrainingConfig& config);
    
    void fineTuneLoRA(const std::string& modelName, Dataset* trainData, 
                      const LoRAConfig& loraConfig, const TrainingConfig& trainConfig);
    
    // Embeddings
    std::vector<float> embed(const std::string& modelName, const std::string& text);
    float similarity(const std::string& modelName, const std::string& text1, const std::string& text2);
    
    // Save model
    void saveModel(const std::string& name, const std::string& path);
    
private:
    std::unordered_map<std::string, LoadedModel> models_;
    mutable std::mutex mutex_;
};

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

// Quick training from raw text
std::string quickTrain(const std::string& text, int vocabSize = 8000, 
                       int dModel = 128, int numLayers = 2, int numHeads = 4,
                       int steps = 1000, int maxTokens = 100, const std::string& prompt = "");

// Quick LoRA fine-tune
std::string quickLoRA(const std::string& baseModelPath, const std::string& trainText, 
                      int loraRank = 16, int steps = 100, const std::string& prompt = "");

} // namespace forge::llm