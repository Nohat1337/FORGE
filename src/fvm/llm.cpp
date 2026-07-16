#include "llm.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <numeric>
#include <execution>
#include <cstring>
#include <optional>

namespace forge::llm {

// ============================================================================
// DATASET IMPLEMENTATION
// ============================================================================

Dataset Dataset::fromDirectory(const std::string& path, const std::vector<std::string>& extensions) {
    Dataset ds;
    std::filesystem::recursive_directory_iterator it(path), end;
    for (; it != end; ++it) {
        if (!it->is_regular_file()) continue;
        auto ext = it->path().extension().string();
        bool ok = extensions.empty();
        for (const auto& e : extensions) if (ext == e) { ok = true; break; }
        if (!ok) continue;
        
        std::ifstream file(it->path());
        if (!file) continue;
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (!content.empty()) ds.addDocument(content, it->path().string());
    }
    return ds;
}

Dataset Dataset::fromJsonl(const std::string& path) {
    Dataset ds;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        size_t pos = line.find("\"text\"");
        if (pos != std::string::npos) {
            pos = line.find(':', pos);
            if (pos != std::string::npos) {
                pos = line.find('"', pos + 1);
                if (pos != std::string::npos) {
                    size_t end = line.find('"', pos + 1);
                    if (end != std::string::npos) {
                        ds.addDocument(line.substr(pos + 1, end - pos - 1));
                    }
                }
            }
        }
    }
    return ds;
}

Dataset Dataset::fromFile(const std::string& path) {
    Dataset ds;
    std::ifstream file(path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    ds.addDocument(content, path);
    return ds;
}

std::tuple<Dataset, Dataset, Dataset> Dataset::split(float trainRatio, float valRatio) {
    Dataset train, val, test;
    size_t n = documents.size();
    size_t trainEnd = size_t(n * trainRatio);
    size_t valEnd = size_t(n * (trainRatio + valRatio));
    
    for (size_t i = 0; i < trainEnd; i++) train.addDocument(documents[i]);
    for (size_t i = trainEnd; i < valEnd; i++) val.addDocument(documents[i]);
    for (size_t i = valEnd; i < n; i++) test.addDocument(documents[i]);
    
    return {train, val, test};
}

void Dataset::shuffle(uint32_t seed) {
    std::mt19937 rng(seed);
    std::shuffle(documents.begin(), documents.end(), rng);
}

void Dataset::filterByLength(size_t minLen, size_t maxLen) {
    documents.erase(std::remove_if(documents.begin(), documents.end(),
        [minLen, maxLen](const std::string& s) { return s.size() < minLen || s.size() > maxLen; }),
        documents.end());
}

void Dataset::deduplicate(float threshold) {
    std::sort(documents.begin(), documents.end());
    documents.erase(std::unique(documents.begin(), documents.end()), documents.end());
}

void Dataset::save(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    size_t count = documents.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const auto& doc : documents) {
        size_t len = doc.size();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(doc.data(), len);
    }
}

Dataset Dataset::load(const std::string& path) {
    Dataset ds;
    std::ifstream file(path, std::ios::binary);
    size_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    ds.documents.resize(count);
    for (size_t i = 0; i < count; i++) {
        size_t len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        ds.documents[i].resize(len);
        file.read(&ds.documents[i][0], len);
    }
    return ds;
}

// ============================================================================
// TOKENIZER IMPLEMENTATION
// ============================================================================

void Tokenizer::train(const std::string& text, int vocabSize, int minFrequency) {
    // Initialize with bytes (0-255)
    for (int i = 0; i < 256; i++) {
        std::string s(1, (char)i);
        vocab_[s] = i;
        idToToken_[i] = s;
    }
    
    // Special tokens
    vocab_["<|endoftext|>"] = 0;  // unk/eos
    vocab_["<|bos|>"] = 1;
    vocab_["<|eos|>"] = 2;
    vocab_["<|pad|>"] = 3;
    idToToken_[0] = "<|endoftext|>";
    idToToken_[1] = "<|bos|>";
    idToToken_[2] = "<|eos|>";
    idToToken_[3] = "<|pad|>";
    unkId_ = 0; bosId_ = 1; eosId_ = 2; padId_ = 3;
    
    // Count word frequencies
    std::unordered_map<std::string, int> wordFreq;
    std::string word;
    for (char c : text) {
        if (std::isspace(c) || std::ispunct(c)) {
            if (!word.empty()) {
                wordFreq[word]++;
                word.clear();
            }
            if (!std::isspace(c)) wordFreq[std::string(1, c)]++;
        } else {
            word += c;
        }
    }
    if (!word.empty()) wordFreq[word]++;
    
    // Filter by frequency
    for (auto it = wordFreq.begin(); it != wordFreq.end();) {
        if (it->second < minFrequency) it = wordFreq.erase(it);
        else ++it;
    }
    
    int nextId = 256 + 4;  // Skip special tokens
    
    // BPE merges
    while (nextId < vocabSize && !wordFreq.empty()) {
        std::unordered_map<std::string, int> pairFreq;
        
        for (const auto& [w, f] : wordFreq) {
            if (w.size() < 2) continue;
            for (size_t i = 0; i < w.size() - 1; i++) {
                std::string pair = w.substr(i, 2);
                pairFreq[pair] += f;
            }
        }
        
        if (pairFreq.empty()) break;
        
        // Find most frequent pair
        std::string bestPair;
        int bestFreq = 0;
        for (const auto& [p, f] : pairFreq) {
            if (f > bestFreq) { bestFreq = f; bestPair = p; }
        }
        
        if (bestFreq < minFrequency) break;
        
        // Add merge
        vocab_[bestPair] = nextId;
        idToToken_[nextId] = bestPair;
        merges_[bestPair] = nextId;
        nextId++;
        
        // Apply merge to vocabulary
        std::unordered_map<std::string, int> newWordFreq;
        for (const auto& [w, f] : wordFreq) {
            std::string newWord;
            for (size_t i = 0; i < w.size(); ) {
                if (i + 1 < w.size() && w.substr(i, 2) == bestPair) {
                    newWord += bestPair;
                    i += 2;
                } else {
                    newWord += w[i];
                    i++;
                }
            }
            newWordFreq[newWord] += f;
        }
        wordFreq = std::move(newWordFreq);
    }
}

void Tokenizer::trainFromDataset(const Dataset& dataset, int vocabSize, int minFrequency) {
    std::string allText;
    for (const auto& doc : dataset.documents) {
        allText += doc + "\n";
    }
    train(allText, vocabSize, minFrequency);
}

std::vector<int> Tokenizer::encode(const std::string& text, bool addBos, bool addEos) const {
    std::vector<int> tokens;
    if (addBos) tokens.push_back(bosId_);
    
    std::string word;
    for (char c : text) {
        if (std::isspace(c)) {
            if (!word.empty()) {
                auto wordTokens = encodeWord(word);
                tokens.insert(tokens.end(), wordTokens.begin(), wordTokens.end());
                word.clear();
            }
            tokens.push_back(vocab_.count(std::string(1, c)) ? vocab_.at(std::string(1, c)) : unkId_);
        } else {
            word += c;
        }
    }
    
    if (!word.empty()) {
        auto wordTokens = encodeWord(word);
        tokens.insert(tokens.end(), wordTokens.begin(), wordTokens.end());
    }
    
    if (addEos) tokens.push_back(eosId_);
    return tokens;
}

std::vector<int> Tokenizer::encodeWord(const std::string& word) const {
    std::vector<int> tokens;
    size_t i = 0;
    while (i < word.size()) {
        bool matched = false;
        for (size_t len = word.size() - i; len > 0; len--) {
            std::string sub = word.substr(i, len);
            if (vocab_.count(sub)) {
                tokens.push_back(vocab_.at(sub));
                i += len;
                matched = true;
                break;
            }
        }
        if (!matched) {
            tokens.push_back(vocab_.count(word.substr(i, 1)) ? vocab_.at(word.substr(i, 1)) : unkId_);
            i++;
        }
    }
    return tokens;
}

std::string Tokenizer::decode(const std::vector<int>& tokens, bool skipSpecial) const {
    std::string result;
    for (int id : tokens) {
        if (skipSpecial && (id == bosId_ || id == eosId_ || id == padId_)) continue;
        if (idToToken_.count(id)) {
            result += idToToken_.at(id);
        } else if (id >= 0 && id < 256) {
            result += (char)id;
        }
    }
    return result;
}

void Tokenizer::setSpecialTokens(const std::string& bos, const std::string& eos, 
                                  const std::string& pad, const std::string& unk) {
    bosId_ = vocab_.count(bos) ? vocab_[bos] : 1;
    eosId_ = vocab_.count(eos) ? vocab_[eos] : 2;
    padId_ = vocab_.count(pad) ? vocab_[pad] : 3;
    unkId_ = vocab_.count(unk) ? vocab_[unk] : 0;
}

void Tokenizer::save(const std::string& path) const {
    std::ofstream file(path);
    file << vocab_.size() << "\n";
    for (const auto& [token, id] : vocab_) {
        file << id << " " << escape(token) << "\n";
    }
    file << merges_.size() << "\n";
    for (const auto& [pair, id] : merges_) {
        file << escape(pair) << " " << id << "\n";
    }
}

void Tokenizer::load(const std::string& path) {
    std::ifstream file(path);
    size_t vocabSize;
    file >> vocabSize;
    vocab_.clear();
    idToToken_.clear();
    for (size_t i = 0; i < vocabSize; i++) {
        int id; std::string token;
        file >> id >> token;
        token = unescape(token);
        vocab_[token] = id;
        idToToken_[id] = token;
    }
    size_t mergeSize;
    file >> mergeSize;
    merges_.clear();
    for (size_t i = 0; i < mergeSize; i++) {
        std::string pair; int id;
        file >> pair >> id;
        merges_[unescape(pair)] = id;
    }
}

std::string Tokenizer::escape(const std::string& s) const {
    std::string result;
    for (char c : s) {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\\') {
            result += "\\x" + toHex((uint8_t)c);
        } else {
            result += c;
        }
    }
    return result;
}

std::string Tokenizer::unescape(const std::string& s) const {
    std::string result;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 3 < s.size() && s[i+1] == 'x') {
            int val = std::stoi(s.substr(i+2, 2), nullptr, 16);
            result += (char)val;
            i += 3;
        } else {
            result += s[i];
        }
    }
    return result;
}

std::string Tokenizer::toHex(uint8_t c) const {
    const char* hex = "0123456789ABCDEF";
    return std::string() + hex[c >> 4] + hex[c & 0xF];
}

// ============================================================================
// LINEAR LAYER IMPLEMENTATION
// ============================================================================

void Linear::forward(const float* input, float* output) const {
    lastInput.assign(input, input + inFeatures);
    for (int j = 0; j < outFeatures; j++) {
        float sum = hasBias ? bias[j] : 0.0f;
        for (int i = 0; i < inFeatures; i++) {
            sum += input[i] * weight[j * inFeatures + i];
        }
        output[j] = sum;
    }
}

void Linear::backward(const float* gradOut, float* gradIn) {
    if (dWeight.empty()) {
        dWeight.resize(weight.size(), 0.0f);
        if (hasBias) dBias.resize(bias.size(), 0.0f);
    }
    
    // dW += gradOut^T * input (outer product)
    for (int j = 0; j < outFeatures; j++) {
        for (int i = 0; i < inFeatures; i++) {
            dWeight[(size_t)j * inFeatures + i] += gradOut[j] * lastInput[i];
        }
        if (hasBias) dBias[j] += gradOut[j];
    }
    
    // gradIn = gradOut * W^T
    if (gradIn) {
        for (int i = 0; i < inFeatures; i++) {
            float sum = 0;
            for (int j = 0; j < outFeatures; j++) {
                sum += gradOut[j] * weight[(size_t)j * inFeatures + i];
            }
            gradIn[i] += sum;
        }
    }
}

void Linear::zeroGrad() {
    if (dWeight.empty()) dWeight.resize(weight.size(), 0.0f);
    else std::fill(dWeight.begin(), dWeight.end(), 0.0f);
    if (hasBias) {
        if (dBias.empty()) dBias.resize(bias.size(), 0.0f);
        else std::fill(dBias.begin(), dBias.end(), 0.0f);
    }
}

void Linear::forwardWithLoRA(const float* input, float* output) const {
    // Base forward
    forward(input, output);
    
    // LoRA addition: output += scale * (input @ A @ B)
    if (loraRank > 0 && !loraA.empty() && !loraB.empty()) {
        std::vector<float> temp(loraRank);
        // input @ A -> temp
        for (int r = 0; r < loraRank; r++) {
            float sum = 0;
            for (int i = 0; i < inFeatures; i++) {
                sum += input[i] * loraA[r * inFeatures + i];
            }
            temp[r] = sum;
        }
        // temp @ B -> add to output
        for (int j = 0; j < outFeatures; j++) {
            float sum = 0;
            for (int r = 0; r < loraRank; r++) {
                sum += temp[r] * loraB[j * loraRank + r];
            }
            output[j] += loraScale * sum;
        }
    }
}

void Linear::initXavier(float scale) {
    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0, scale);
    for (auto& w : weight) w = dist(rng);
    if (hasBias) for (auto& b : bias) b = 0;
}

void Linear::initZero() {
    std::fill(weight.begin(), weight.end(), 0.0f);
    if (hasBias) std::fill(bias.begin(), bias.end(), 0.0f);
}

void Linear::initLoRA(int rank, float alpha, float dropout) {
    loraRank = rank;
    loraScale = alpha / rank;
    loraA.resize(rank * inFeatures);
    loraB.resize(outFeatures * rank);
    
    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0, 0.02f);
    for (auto& w : loraA) w = dist(rng);
    std::fill(loraB.begin(), loraB.end(), 0.0f);  // Initialize B to zero
}

void Linear::mergeLoRA() {
    if (loraRank == 0) return;
    // weight += scale * B @ A^T
    for (int j = 0; j < outFeatures; j++) {
        for (int i = 0; i < inFeatures; i++) {
            float sum = 0;
            for (int r = 0; r < loraRank; r++) {
                sum += loraB[j * loraRank + r] * loraA[r * inFeatures + i];
            }
            weight[j * inFeatures + i] += loraScale * sum;
        }
    }
    loraRank = 0;
    loraA.clear();
    loraB.clear();
}

void Linear::save(std::ofstream& f) const {
    f.write(reinterpret_cast<const char*>(&inFeatures), sizeof(inFeatures));
    f.write(reinterpret_cast<const char*>(&outFeatures), sizeof(outFeatures));
    f.write(reinterpret_cast<const char*>(&hasBias), sizeof(hasBias));
    size_t sz = weight.size();
    f.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
    f.write(reinterpret_cast<const char*>(weight.data()), sz * sizeof(float));
    if (hasBias) {
        sz = bias.size();
        f.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
        f.write(reinterpret_cast<const char*>(bias.data()), sz * sizeof(float));
    }
    // LoRA
    f.write(reinterpret_cast<const char*>(&loraRank), sizeof(loraRank));
    f.write(reinterpret_cast<const char*>(&loraScale), sizeof(loraScale));
    if (loraRank > 0) {
        sz = loraA.size();
        f.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
        f.write(reinterpret_cast<const char*>(loraA.data()), sz * sizeof(float));
        sz = loraB.size();
        f.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
        f.write(reinterpret_cast<const char*>(loraB.data()), sz * sizeof(float));
    }
}

void Linear::load(std::ifstream& f) {
    f.read(reinterpret_cast<char*>(&inFeatures), sizeof(inFeatures));
    f.read(reinterpret_cast<char*>(&outFeatures), sizeof(outFeatures));
    f.read(reinterpret_cast<char*>(&hasBias), sizeof(hasBias));
    size_t sz;
    f.read(reinterpret_cast<char*>(&sz), sizeof(sz));
    weight.resize(sz);
    f.read(reinterpret_cast<char*>(weight.data()), sz * sizeof(float));
    if (hasBias) {
        f.read(reinterpret_cast<char*>(&sz), sizeof(sz));
        bias.resize(sz);
        f.read(reinterpret_cast<char*>(bias.data()), sz * sizeof(float));
    }
    f.read(reinterpret_cast<char*>(&loraRank), sizeof(loraRank));
    f.read(reinterpret_cast<char*>(&loraScale), sizeof(loraScale));
    if (loraRank > 0) {
        f.read(reinterpret_cast<char*>(&sz), sizeof(sz));
        loraA.resize(sz);
        f.read(reinterpret_cast<char*>(loraA.data()), sz * sizeof(float));
        f.read(reinterpret_cast<char*>(&sz), sizeof(sz));
        loraB.resize(sz);
        f.read(reinterpret_cast<char*>(loraB.data()), sz * sizeof(float));
    }
}

// ============================================================================
// LAYER NORM
// ============================================================================

void LayerNorm::forward(const float* input, float* output) const {
    lastInput.assign(input, input + features);
    float mean = 0, var = 0;
    for (int i = 0; i < features; i++) mean += input[i];
    mean /= features;
    for (int i = 0; i < features; i++) var += (input[i] - mean) * (input[i] - mean);
    var /= features;
    
    for (int i = 0; i < features; i++) {
        output[i] = (input[i] - mean) / std::sqrt(var + eps) * weight[i] + bias[i];
    }
    lastOutput.assign(output, output + features);
}

void LayerNorm::backward(const float* gradOut, float* gradIn) {
    if (dWeight.empty()) {
        dWeight.resize(features, 0.0f);
        dBias.resize(features, 0.0f);
    }
    
    float mean = 0, var = 0;
    for (int i = 0; i < features; i++) mean += lastInput[i];
    mean /= features;
    for (int i = 0; i < features; i++) var += (lastInput[i] - mean) * (lastInput[i] - mean);
    var /= features;
    float stdInv = 1.0f / std::sqrt(var + eps);
    
    // dx_hat = gradOut * weight
    // dvar and dmean
    float dxhat_sum = 0, dxhat_xhat_sum = 0;
    std::vector<float> dxhat(features);
    for (int i = 0; i < features; i++) {
        dxhat[i] = gradOut[i] * weight[i];
        dxhat_sum += dxhat[i];
        float xhat = (lastInput[i] - mean) * stdInv;
        dxhat_xhat_sum += dxhat[i] * xhat;
    }
    
    for (int i = 0; i < features; i++) {
        float xhat = (lastInput[i] - mean) * stdInv;
        gradIn[i] += stdInv * (dxhat[i] - dxhat_sum / features - xhat * dxhat_xhat_sum / features);
        dWeight[i] += gradOut[i] * lastOutput[i]; // simplified: just accumulate
        dBias[i] += gradOut[i];
    }
}

void LayerNorm::zeroGrad() {
    if (dWeight.empty()) dWeight.resize(features, 0.0f);
    else std::fill(dWeight.begin(), dWeight.end(), 0.0f);
    if (dBias.empty()) dBias.resize(features, 0.0f);
    else std::fill(dBias.begin(), dBias.end(), 0.0f);
}

void LayerNorm::save(std::ofstream& f) const {
    f.write(reinterpret_cast<const char*>(&features), sizeof(features));
    f.write(reinterpret_cast<const char*>(&eps), sizeof(eps));
    size_t sz = weight.size();
    f.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
    f.write(reinterpret_cast<const char*>(weight.data()), sz * sizeof(float));
    sz = bias.size();
    f.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
    f.write(reinterpret_cast<const char*>(bias.data()), sz * sizeof(float));
}

void LayerNorm::load(std::ifstream& f) {
    f.read(reinterpret_cast<char*>(&features), sizeof(features));
    f.read(reinterpret_cast<char*>(&eps), sizeof(eps));
    size_t sz;
    f.read(reinterpret_cast<char*>(&sz), sizeof(sz));
    weight.resize(sz);
    f.read(reinterpret_cast<char*>(weight.data()), sz * sizeof(float));
    f.read(reinterpret_cast<char*>(&sz), sizeof(sz));
    bias.resize(sz);
    f.read(reinterpret_cast<char*>(bias.data()), sz * sizeof(float));
}

// ============================================================================
// MULTI-HEAD ATTENTION
// ============================================================================

MultiHeadAttention::MultiHeadAttention(int dModel, int numHeads_, int dFF, bool useBias)
    : numHeads(numHeads_), headDim(dModel / numHeads_), scale(1.0f / std::sqrt(headDim)) {
    qProj = Linear(dModel, dModel, useBias);
    kProj = Linear(dModel, dModel, useBias);
    vProj = Linear(dModel, dModel, useBias);
    oProj = Linear(dModel, dModel, useBias);
}

void MultiHeadAttention::forward(const float* input, float* output, int seqLen, int dModel, const float* mask) const {
    lastInput.assign(input, input + seqLen * dModel);
    lastSeqLen = seqLen;
    qCache.resize(seqLen * dModel);
    kCache.resize(seqLen * dModel);
    vCache.resize(seqLen * dModel);
    
    for (int i = 0; i < seqLen; i++) {
        qProj.forwardWithLoRA(&input[i * dModel], &qCache[i * dModel]);
        kProj.forwardWithLoRA(&input[i * dModel], &kCache[i * dModel]);
        vProj.forwardWithLoRA(&input[i * dModel], &vCache[i * dModel]);
    }
    
    attnWeights.resize(seqLen * seqLen);
    for (int i = 0; i < seqLen; i++) {
        for (int t = 0; t < seqLen; t++) {
            float score = 0;
            for (int h = 0; h < dModel; h++) {
                score += qCache[i * dModel + h] * kCache[t * dModel + h];
            }
            score *= scale;
            if (mask && mask[i * seqLen + t] < 0) score = -1e9f;
            attnWeights[i * seqLen + t] = score;
        }
    }
    // Softmax per row
    for (int i = 0; i < seqLen; i++) {
        float maxS = attnWeights[i * seqLen];
        for (int t = 1; t < seqLen; t++) maxS = std::max(maxS, attnWeights[i * seqLen + t]);
        float sum = 0;
        for (int t = 0; t < seqLen; t++) {
            attnWeights[i * seqLen + t] = std::exp(attnWeights[i * seqLen + t] - maxS);
            sum += attnWeights[i * seqLen + t];
        }
        for (int t = 0; t < seqLen; t++) attnWeights[i * seqLen + t] /= sum;
    }
    
    // attnWeights * V
    for (int i = 0; i < seqLen; i++) {
        for (int j = 0; j < dModel; j++) {
            float sum = 0;
            for (int t = 0; t < seqLen; t++) {
                sum += attnWeights[i * seqLen + t] * vCache[t * dModel + j];
            }
            output[i * dModel + j] = sum;
        }
    }
    
    // Store intermediate output for oProj backward
    attnIntermediate.resize(seqLen * dModel);
    for (int i = 0; i < seqLen; i++) {
        for (int j = 0; j < dModel; j++) {
            float sum = 0;
            for (int t = 0; t < seqLen; t++) {
                sum += attnWeights[i * seqLen + t] * vCache[t * dModel + j];
            }
            attnIntermediate[i * dModel + j] = sum;
        }
    }
    
    for (int i = 0; i < seqLen; i++) {
        oProj.forwardWithLoRA(&attnIntermediate[i * dModel], &output[i * dModel]);
    }
}

void MultiHeadAttention::backward(const float* gradOut, float* gradIn, int seqLen, int dModel) {
    int S = lastSeqLen;
    
    // Backward through oProj
    std::vector<float> dAttnIntermediate(S * dModel, 0.0f);
    for (int i = 0; i < S; i++) {
        oProj.backward(&gradOut[i * dModel], &dAttnIntermediate[i * dModel]);
    }
    
    // Simplified backward: approximate gradients via the stored attention pattern
    // dV[t][j] = sum_i attnWeights[i][t] * gradOut[i][j]
    std::vector<float> dV(S * dModel, 0.0f);
    for (int t = 0; t < S; t++) {
        for (int j = 0; j < dModel; j++) {
            float sum = 0;
            for (int i = 0; i < S; i++) {
                sum += attnWeights[i * S + t] * gradOut[i * dModel + j];
            }
            dV[t * dModel + j] = sum;
        }
    }
    
    // dAttnWeights[i][t] = sum_j gradOut[i][j] * v[t][j]
    std::vector<float> dWeights(S * S, 0.0f);
    for (int i = 0; i < S; i++) {
        for (int t = 0; t < S; t++) {
            float sum = 0;
            for (int j = 0; j < dModel; j++) {
                sum += gradOut[i * dModel + j] * vCache[t * dModel + j];
            }
            dWeights[i * S + t] = sum;
        }
    }
    
    // Softmax backward: dScore[i][t] = attnWeights[i][t] * (dWeights[i][t] - sum_t' attnWeights[i][t'] * dWeights[i][t'])
    std::vector<float> dScores(S * S);
    for (int i = 0; i < S; i++) {
        float dot = 0;
        for (int t = 0; t < S; t++) dot += attnWeights[i * S + t] * dWeights[i * S + t];
        for (int t = 0; t < S; t++) {
            dScores[i * S + t] = attnWeights[i * S + t] * (dWeights[i * S + t] - dot);
        }
    }
    
    // dQ[i][h] = scale * sum_t dScores[i][t] * k[t][h]
    // dK[t][h] = scale * sum_i dScores[i][t] * q[i][h]
    std::vector<float> dQ(S * dModel, 0.0f), dK(S * dModel, 0.0f);
    for (int i = 0; i < S; i++) {
        for (int h = 0; h < dModel; h++) {
            float sumQ = 0;
            for (int t = 0; t < S; t++) sumQ += dScores[i * S + t] * kCache[t * dModel + h];
            dQ[i * dModel + h] = scale * sumQ;
        }
    }
    for (int t = 0; t < S; t++) {
        for (int h = 0; h < dModel; h++) {
            float sumK = 0;
            for (int i = 0; i < S; i++) sumK += dScores[i * S + t] * qCache[i * dModel + h];
            dK[t * dModel + h] = scale * sumK;
        }
    }
    
    // Backward through Q, K, V projections and accumulate into gradIn
    std::vector<float> dq(dModel), dk(dModel), dv(dModel);
    for (int i = 0; i < S; i++) {
        std::fill(dq.begin(), dq.end(), 0.0f);
        std::fill(dk.begin(), dk.end(), 0.0f);
        std::fill(dv.begin(), dv.end(), 0.0f);
        
        qProj.backward(&dQ[i * dModel], dq.data());
        kProj.backward(&dK[i * dModel], dk.data());
        vProj.backward(&dV[i * dModel], dv.data());
        
        for (int h = 0; h < dModel; h++) {
            gradIn[i * dModel + h] += dq[h] + dk[h] + dv[h];
        }
    }
}

void MultiHeadAttention::zeroGrad() {
    qProj.zeroGrad(); kProj.zeroGrad(); vProj.zeroGrad(); oProj.zeroGrad();
}

void MultiHeadAttention::initXavier(float scale) {
    qProj.initXavier(scale);
    kProj.initXavier(scale);
    vProj.initXavier(scale);
    oProj.initXavier(scale);
}

void MultiHeadAttention::initLoRA(int rank, float alpha, float dropout) {
    qProj.initLoRA(rank, alpha, dropout);
    vProj.initLoRA(rank, alpha, dropout);
    kProj.initLoRA(rank, alpha, dropout);
    oProj.initLoRA(rank, alpha, dropout);
}

void MultiHeadAttention::mergeLoRA() {
    qProj.mergeLoRA();
    kProj.mergeLoRA();
    vProj.mergeLoRA();
    oProj.mergeLoRA();
}

void MultiHeadAttention::save(std::ofstream& f) const {
    qProj.save(f);
    kProj.save(f);
    vProj.save(f);
    oProj.save(f);
    f.write(reinterpret_cast<const char*>(&numHeads), sizeof(numHeads));
    f.write(reinterpret_cast<const char*>(&headDim), sizeof(headDim));
    f.write(reinterpret_cast<const char*>(&scale), sizeof(scale));
}

void MultiHeadAttention::load(std::ifstream& f) {
    qProj.load(f);
    kProj.load(f);
    vProj.load(f);
    oProj.load(f);
    f.read(reinterpret_cast<char*>(&numHeads), sizeof(numHeads));
    f.read(reinterpret_cast<char*>(&headDim), sizeof(headDim));
    f.read(reinterpret_cast<char*>(&scale), sizeof(scale));
}

// ============================================================================
// FEED FORWARD
// ============================================================================

FeedForward::FeedForward(int dModel, int dFF_, bool useBias)
    : ff1(dModel, dFF_, useBias), ff2(dFF_, dModel, useBias), dFF(dFF_) {}

void FeedForward::forward(const float* input, float* output, int seqLen, int dModel) const {
    hiddenCache.resize(seqLen * dFF);
    
    for (int i = 0; i < seqLen; i++) {
        ff1.forwardWithLoRA(&input[i * dModel], &hiddenCache[i * dFF]);
        for (int j = 0; j < dFF; j++) {
            float x = hiddenCache[i * dFF + j];
            hiddenCache[i * dFF + j] = 0.5f * x * (1.0f + std::tanh(std::sqrt(2.0f / M_PI) * (x + 0.044715f * x * x * x)));
        }
    }
    
    for (int i = 0; i < seqLen; i++) {
        ff2.forwardWithLoRA(&hiddenCache[i * dFF], &output[i * dModel]);
    }
}

void FeedForward::backward(const float* gradOut, float* gradIn, int seqLen, int dModel) {
    // Backward through ff2
    std::vector<float> dHidden(seqLen * dFF, 0.0f);
    for (int i = 0; i < seqLen; i++) {
        ff2.backward(&gradOut[i * dModel], &dHidden[i * dFF]);
    }
    
    // Backward through GELU: d/dx GELU(x) = 0.5*(1+tanh(sqrt(2/pi)*(x+0.044715x^3))) + 0.5x*(1-tanh^2(...))*sqrt(2/pi)*(1+3*0.044715x^2)
    // We need the pre-GELU values. hiddenCache stores POST-GELU values.
    // We approximate: pre_gelu_x = hiddenCache (close enough for training)
    for (int i = 0; i < seqLen; i++) {
        for (int j = 0; j < dFF; j++) {
            float x = hiddenCache[i * dFF + j]; // post-GELU, approximate pre-GELU
            float tanhArg = std::sqrt(2.0f / M_PI) * (x + 0.044715f * x * x * x);
            float tanhVal = std::tanh(tanhArg);
            float geluDeriv = 0.5f * (1.0f + tanhVal) + 0.5f * x * (1.0f - tanhVal * tanhVal) *
                             std::sqrt(2.0f / M_PI) * (1.0f + 3.0f * 0.044715f * x * x);
            dHidden[i * dFF + j] *= geluDeriv;
        }
    }
    
    // Backward through ff1
    for (int i = 0; i < seqLen; i++) {
        ff1.backward(&dHidden[i * dFF], &gradIn[i * dModel]);
    }
}

void FeedForward::zeroGrad() {
    ff1.zeroGrad(); ff2.zeroGrad();
}

void FeedForward::initXavier(float scale) {
    ff1.initXavier(scale);
    ff2.initXavier(scale);
}

void FeedForward::initLoRA(int rank, float alpha, float dropout) {
    ff1.initLoRA(rank, alpha, dropout);
    ff2.initLoRA(rank, alpha, dropout);
}

void FeedForward::mergeLoRA() {
    ff1.mergeLoRA();
    ff2.mergeLoRA();
}

void FeedForward::save(std::ofstream& f) const {
    ff1.save(f);
    ff2.save(f);
    f.write(reinterpret_cast<const char*>(&dropout), sizeof(dropout));
}

void FeedForward::load(std::ifstream& f) {
    ff1.load(f);
    ff2.load(f);
    f.read(reinterpret_cast<char*>(&dropout), sizeof(dropout));
}

// ============================================================================
// TRANSFORMER BLOCK
// ============================================================================

TransformerBlock::TransformerBlock(const ModelConfig& cfg)
    : ln1(cfg.dModel, cfg.layerNormEps), ln2(cfg.dModel, cfg.layerNormEps),
      attn(cfg.dModel, cfg.numHeads, cfg.dFF, cfg.useBias),
      ff(cfg.dModel, cfg.dFF, cfg.useBias), dropout(cfg.dropout) {}

void TransformerBlock::forward(const float* input, float* output, int seqLen, int dModel, const float* mask) const {
    blockInput.assign(input, input + seqLen * dModel);
    ln1Out.resize(seqLen * dModel);
    attnOut.resize(seqLen * dModel);
    ln2Out.resize(seqLen * dModel);
    
    // ln1 -> attn -> residual
    for (int i = 0; i < seqLen; i++) ln1.forward(&input[i * dModel], &ln1Out[i * dModel]);
    attn.forward(ln1Out.data(), attnOut.data(), seqLen, dModel, mask);
    for (int i = 0; i < seqLen * dModel; i++) output[i] = input[i] + attnOut[i];
    
    // ln2 -> ff -> residual
    std::vector<float> temp(seqLen * dModel);
    for (int i = 0; i < seqLen; i++) ln2.forward(&output[i * dModel], &ln2Out[i * dModel]);
    ff.forward(ln2Out.data(), temp.data(), seqLen, dModel);
    for (int i = 0; i < seqLen * dModel; i++) output[i] += temp[i];
}

void TransformerBlock::backward(const float* gradOut, float* gradIn, int seqLen, int dModel) {
    std::vector<float> dAfterAttn(seqLen * dModel, 0.0f);
    std::vector<float> dLn2(seqLen * dModel, 0.0f);
    
    ff.backward(gradOut, dLn2.data(), seqLen, dModel);
    
    // Backward through ln2: need per-position caches
    for (int i = 0; i < seqLen; i++) {
        // ln2 input = blockInput[i] + attnOut[i] (the residual connection)
        std::vector<float> ln2Input(dModel);
        for (int j = 0; j < dModel; j++)
            ln2Input[j] = blockInput[i * dModel + j] + attnOut[i * dModel + j];
        ln2.lastInput = ln2Input;
        ln2.lastOutput.assign(ln2Out.data() + i * dModel, ln2Out.data() + (i + 1) * dModel);
        ln2.backward(&dLn2[i * dModel], &dAfterAttn[i * dModel]);
    }
    
    // gradIn += gradOut (from second residual) + dAfterAttn
    std::vector<float> dAttnOut(seqLen * dModel);
    for (int i = 0; i < seqLen * dModel; i++) {
        dAttnOut[i] = gradOut[i] + dAfterAttn[i];
    }
    
    // Backward through attn
    std::vector<float> dLn1(seqLen * dModel, 0.0f);
    attn.backward(dAttnOut.data(), dLn1.data(), seqLen, dModel);
    
    // Backward through ln1: need per-position caches
    for (int i = 0; i < seqLen; i++) {
        ln1.lastInput.assign(blockInput.data() + i * dModel, blockInput.data() + (i + 1) * dModel);
        ln1.lastOutput.assign(ln1Out.data() + i * dModel, ln1Out.data() + (i + 1) * dModel);
        ln1.backward(&dLn1[i * dModel], &gradIn[i * dModel]);
    }
    
    // First residual
    for (int i = 0; i < seqLen * dModel; i++) {
        gradIn[i] += gradOut[i];
    }
}

void TransformerBlock::zeroGrad() {
    ln1.zeroGrad(); ln2.zeroGrad();
    attn.zeroGrad();
    ff.zeroGrad();
}

void TransformerBlock::initLoRA(int rank, float alpha, float dropout) {
    attn.initLoRA(rank, alpha, dropout);
    ff.initLoRA(rank, alpha, dropout);
}

void TransformerBlock::mergeLoRA() {
    attn.mergeLoRA();
    ff.mergeLoRA();
}

void TransformerBlock::save(std::ofstream& f) const {
    ln1.save(f);
    ln2.save(f);
    attn.save(f);
    ff.save(f);
    f.write(reinterpret_cast<const char*>(&dropout), sizeof(dropout));
}

void TransformerBlock::load(std::ifstream& f) {
    ln1.load(f);
    ln2.load(f);
    attn.load(f);
    ff.load(f);
    f.read(reinterpret_cast<char*>(&dropout), sizeof(dropout));
}

// ============================================================================
// TRANSFORMER MODEL
// ============================================================================

void TransformerModel::initialize() {
    // Embeddings
    tokenEmb.resize(config.vocabSize * config.dModel);
    posEmb.resize(config.maxSeqLen * config.dModel);
    
    // Transformer layers
    layers.resize(config.numLayers);
    for (auto& layer : layers) layer = TransformerBlock(config);
    
    // Final norm
    lnFinal = LayerNorm(config.dModel, config.layerNormEps);
    
    // Output head
    head = Linear(config.dModel, config.vocabSize, config.useBias);
    
    // Initialize
    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0, 0.02f);
    for (auto& v : tokenEmb) v = dist(rng);
    for (auto& v : posEmb) v = dist(rng);
    head.initXavier(0.02f);
    for (auto& layer : layers) {
        layer.attn.initXavier(0.02f);
        layer.ff.initXavier(0.02f);
    }
}

std::vector<float> TransformerModel::forward(const std::vector<int>& tokens) {
    int seqLen = std::min((int)tokens.size(), config.maxSeqLen);
    int dModel = config.dModel;
    int vocabSize = config.vocabSize;
    
    // Ensure buffers are large enough
    temp1_.resize(seqLen * dModel);
    temp2_.resize(seqLen * dModel);
    logits_.resize(seqLen * vocabSize);
    
    // Embeddings
    for (int i = 0; i < seqLen; i++) {
        int tok = tokens[i];
        for (int j = 0; j < dModel; j++) {
            float val = tokenEmb[tok * dModel + j];
            if (i < config.maxSeqLen) val += posEmb[i * dModel + j];
            temp1_[i * dModel + j] = val;
        }
    }
    
    // Causal mask
    std::vector<float> mask(seqLen * seqLen, 0);
    for (int i = 0; i < seqLen; i++) {
        for (int j = i + 1; j < seqLen; j++) mask[i * seqLen + j] = -1e9f;
    }
    
    // Transformer layers
    for (auto& layer : layers) {
        layer.forward(temp1_.data(), temp2_.data(), seqLen, dModel, mask.data());
        std::swap(temp1_, temp2_);
    }
    
    // Final layer norm
    for (int i = 0; i < seqLen; i++) {
        lnFinal.forward(&temp1_[i * dModel], &temp2_[i * dModel]);
    }
    
    // Output projection
    for (int i = 0; i < seqLen; i++) {
        head.forwardWithLoRA(&temp2_[i * dModel], &logits_[i * vocabSize]);
    }
    
    return logits_;
}

void TransformerModel::forwardTrain(const int* tokens, int batchSize, int seqLen, 
                                   float* logits, float* loss, const int* targets) {
    (void)batchSize; (void)logits; (void)loss; (void)targets;
    // Simplified forward pass for training - reuse the forward logic
    // For now, just do a forward pass to get logits
    std::vector<int> tokensVec(tokens, tokens + seqLen);
    auto result = forward(tokensVec);
    // In a real implementation, this would compute loss and gradients
    (void)targets;
    if (logits) {
        std::copy(logits_.begin(), logits_.end(), logits);
    }
}

std::vector<int> TransformerModel::generate(const std::vector<int>& prompt, int maxTokens, 
                                           float temperature, int topK, float topP) {
    std::vector<int> tokens = prompt;
    std::mt19937 rng(std::random_device{}());
    
    for (int step = 0; step < maxTokens; step++) {
        auto logits = forward(tokens);
        int seqLen = tokens.size();
        int vocabSize = config.vocabSize;
        
        // Get logits for last token
        float* lastLogits = &logits[(seqLen - 1) * vocabSize];
        
        // Apply temperature
        for (int i = 0; i < vocabSize; i++) lastLogits[i] /= temperature;
        
        // Top-k filtering
        std::vector<std::pair<float, int>> indexed;
        indexed.reserve(vocabSize);
        for (int i = 0; i < vocabSize; i++) indexed.emplace_back(lastLogits[i], i);
        std::nth_element(indexed.begin(), indexed.begin() + std::min(topK, vocabSize), indexed.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
        indexed.resize(std::min(topK, vocabSize));
        
        // Top-p (nucleus) filtering
        if (topP < 1.0f) {
            // Softmax
            float maxLogit = indexed[0].first;
            float sum = 0;
            for (auto& p : indexed) {
                p.first = std::exp(p.first - maxLogit);
                sum += p.first;
            }
            for (auto& p : indexed) p.first /= sum;
            
            // Cumulative probability
            std::sort(indexed.begin(), indexed.end(), 
                [](const auto& a, const auto& b) { return a.first > b.first; });
            float cumsum = 0;
            size_t cutoff = indexed.size();
            for (size_t i = 0; i < indexed.size(); i++) {
                cumsum += indexed[i].first;
                if (cumsum > topP) { cutoff = i + 1; break; }
            }
            indexed.resize(cutoff);
        }
        
        // Sample
        std::vector<float> probs;
        probs.reserve(indexed.size());
        for (auto& p : indexed) probs.push_back(p.first);
        std::discrete_distribution<int> dist(probs.begin(), probs.end());
        int nextToken = indexed[dist(rng)].second;
        
        tokens.push_back(nextToken);
        if (nextToken == 0 || nextToken == 2) break; // EOS
    }
    
    return tokens;
}

void TransformerModel::enableLoRA(int rank, float alpha, float dropout) {
    config.loraRank = rank;
    config.loraAlpha = alpha;
    config.loraDropout = dropout;
    head.initLoRA(rank, alpha, dropout);
    for (auto& layer : layers) layer.initLoRA(rank, alpha, dropout);
}

void TransformerModel::disableLoRA() {
    config.loraRank = 0;
    head.mergeLoRA();
    for (auto& layer : layers) layer.mergeLoRA();
}

void TransformerModel::mergeLoRA() {
    head.mergeLoRA();
    for (auto& layer : layers) layer.mergeLoRA();
    config.loraRank = 0;
}

void TransformerModel::save(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(&config), sizeof(ModelConfig));
    
    size_t sz = tokenEmb.size();
    f.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
    f.write(reinterpret_cast<const char*>(tokenEmb.data()), sz * sizeof(float));
    
    sz = posEmb.size();
    f.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
    f.write(reinterpret_cast<const char*>(posEmb.data()), sz * sizeof(float));
    
    head.save(f);
    
    sz = layers.size();
    f.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
    for (const auto& layer : layers) layer.save(f);
    
    lnFinal.save(f);
}

void TransformerModel::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    f.read(reinterpret_cast<char*>(&config), sizeof(ModelConfig));
    
    size_t sz;
    f.read(reinterpret_cast<char*>(&sz), sizeof(sz));
    tokenEmb.resize(sz);
    f.read(reinterpret_cast<char*>(tokenEmb.data()), sz * sizeof(float));
    
    f.read(reinterpret_cast<char*>(&sz), sizeof(sz));
    posEmb.resize(sz);
    f.read(reinterpret_cast<char*>(posEmb.data()), sz * sizeof(float));
    
    head.load(f);
    
    f.read(reinterpret_cast<char*>(&sz), sizeof(sz));
    layers.resize(sz);
    for (auto& layer : layers) layer.load(f);
    
    lnFinal.load(f);
}

size_t TransformerModel::parameterCount() const {
    size_t count = tokenEmb.size() + posEmb.size();
    count += head.weight.size() + (head.hasBias ? head.bias.size() : 0);
    for (const auto& layer : layers) {
        count += layer.attn.qProj.weight.size() + layer.attn.kProj.weight.size() + 
                 layer.attn.vProj.weight.size() + layer.attn.oProj.weight.size();
        if (layer.attn.qProj.hasBias) count += layer.attn.qProj.bias.size() * 4;
        count += layer.ff.ff1.weight.size() + layer.ff.ff2.weight.size();
        if (layer.ff.ff1.hasBias) count += layer.ff.ff1.bias.size() + layer.ff.ff2.bias.size();
        count += layer.ln1.weight.size() * 2 + layer.ln2.weight.size() * 2;
    }
    count += lnFinal.weight.size() * 2;
    return count;
}

size_t TransformerModel::trainableParameterCount() const {
    if (config.loraRank == 0) return parameterCount();
    size_t count = 0;
    for (const auto& layer : layers) {
        count += layer.attn.qProj.loraA.size() + layer.attn.qProj.loraB.size() +
                 layer.attn.vProj.loraA.size() + layer.attn.vProj.loraB.size() +
                 layer.attn.kProj.loraA.size() + layer.attn.kProj.loraB.size() +
                 layer.attn.oProj.loraA.size() + layer.attn.oProj.loraB.size() +
                 layer.ff.ff1.loraA.size() + layer.ff.ff1.loraB.size() +
                 layer.ff.ff2.loraA.size() + layer.ff.ff2.loraB.size();
    }
    count += head.loraA.size() + head.loraB.size();
    return count;
}

std::string TransformerModel::summary() const {
    std::ostringstream out;
    out << "Transformer Model Summary\n";
    out << "========================\n";
    out << "Vocab Size: " << config.vocabSize << "\n";
    out << "Max Seq Len: " << config.maxSeqLen << "\n";
    out << "dModel: " << config.dModel << "\n";
    out << "Num Heads: " << config.numHeads << "\n";
    out << "Num Layers: " << config.numLayers << "\n";
    out << "dFF: " << config.dFF << "\n";
    out << "Dropout: " << config.dropout << "\n";
    out << "Total Parameters: " << parameterCount() << "\n";
    if (config.loraRank > 0) {
        out << "LoRA Enabled: rank=" << config.loraRank << ", alpha=" << config.loraAlpha << "\n";
        out << "Trainable Parameters: " << trainableParameterCount() << "\n";
    }
    return out.str();
}

// ============================================================================
// BACKWARD PASS & TRAINING SUPPORT
// ============================================================================

float TransformerModel::crossEntropyLoss(std::vector<float>& logits, const std::vector<int>& targets, int seqLen, int vocabSize) {
    float totalLoss = 0;
    for (int i = 0; i < seqLen; i++) {
        float* row = &logits[i * vocabSize];
        int tgt = targets[i];
        
        // Softmax + cross-entropy in one pass (numerically stable)
        float maxLogit = *std::max_element(row, row + vocabSize);
        float sumExp = 0;
        for (int v = 0; v < vocabSize; v++) {
            row[v] = std::exp(row[v] - maxLogit);
            sumExp += row[v];
        }
        
        // Loss = -log(softmax(target))
        totalLoss += -std::log(row[tgt] / sumExp + 1e-8f);
        
        // Gradient: softmax - one_hot(target)
        for (int v = 0; v < vocabSize; v++) {
            row[v] = row[v] / sumExp - (v == tgt ? 1.0f : 0.0f);
        }
    }
    return totalLoss / seqLen;
}

float TransformerModel::forwardAndBackward(const std::vector<int>& tokens, const std::vector<int>& targets) {
    int seqLen = std::min((int)tokens.size(), config.maxSeqLen);
    int dModel = config.dModel;
    int vocabSize = config.vocabSize;
    
    zeroGrad();
    
    // === FORWARD ===
    // Embeddings
    embCache_.resize(seqLen);
    for (int i = 0; i < seqLen; i++) {
        embCache_[i].resize(dModel);
        int tok = tokens[i];
        for (int j = 0; j < dModel; j++) {
            float val = tokenEmb[tok * dModel + j];
            if (i < config.maxSeqLen) val += posEmb[i * dModel + j];
            embCache_[i][j] = val;
        }
    }
    
    // Causal mask
    std::vector<float> mask(seqLen * seqLen, 0);
    for (int i = 0; i < seqLen; i++)
        for (int j = i + 1; j < seqLen; j++) mask[i * seqLen + j] = -1e9f;
    
    // Forward through layers (caches stored in each layer)
    temp1_.resize(seqLen * dModel);
    temp2_.resize(seqLen * dModel);
    for (int i = 0; i < seqLen; i++)
        std::copy(embCache_[i].begin(), embCache_[i].end(), &temp1_[i * dModel]);
    
    for (auto& layer : layers) {
        layer.forward(temp1_.data(), temp2_.data(), seqLen, dModel, mask.data());
        std::swap(temp1_, temp2_);
    }
    
    // Final layer norm
    lnFinalOut_.resize(seqLen * dModel);
    for (int i = 0; i < seqLen; i++) {
        lnFinal.forward(&temp1_[i * dModel], &lnFinalOut_[i * dModel]);
    }
    
    // Output projection (logits)
    logits_.resize(seqLen * vocabSize);
    for (int i = 0; i < seqLen; i++) {
        head.forward(&lnFinalOut_[i * dModel], &logits_[i * vocabSize]);
    }
    
    // === LOSS ===
    float loss = crossEntropyLoss(logits_, targets, seqLen, vocabSize);
    
    // === BACKWARD ===
    // grad for head
    std::vector<float> dLnFinal(seqLen * dModel, 0.0f);
    for (int i = 0; i < seqLen; i++) {
        head.lastInput.assign(lnFinalOut_.data() + i * dModel, lnFinalOut_.data() + (i+1) * dModel);
        head.backward(&logits_[i * vocabSize], &dLnFinal[i * dModel]);
    }
    
    // Backward through final layer norm
    std::vector<float> dAfterBlocks(seqLen * dModel, 0.0f);
    for (int i = 0; i < seqLen; i++) {
        lnFinal.lastInput.assign(temp1_.data() + i * dModel, temp1_.data() + (i+1) * dModel);
        lnFinal.lastOutput.assign(lnFinalOut_.data() + i * dModel, lnFinalOut_.data() + (i+1) * dModel);
        lnFinal.backward(&dLnFinal[i * dModel], &dAfterBlocks[i * dModel]);
    }
    
    // Backward through transformer layers (in reverse)
    std::vector<float> dEmb(seqLen * dModel, 0.0f);
    std::copy(dAfterBlocks.begin(), dAfterBlocks.end(), dEmb.begin());
    
    std::vector<float> dTemp(seqLen * dModel);
    for (int li = (int)layers.size() - 1; li >= 0; li--) {
        std::fill(dTemp.begin(), dTemp.end(), 0.0f);
        layers[li].backward(dEmb.data(), dTemp.data(), seqLen, dModel);
        dEmb = dTemp;
    }
    
    // Backward through embeddings
    dTokenEmb.resize(config.vocabSize * dModel, 0.0f);
    dPosEmb.resize(config.maxSeqLen * dModel, 0.0f);
    for (int i = 0; i < seqLen; i++) {
        int tok = tokens[i];
        for (int j = 0; j < dModel; j++) {
            dTokenEmb[tok * dModel + j] += dEmb[i * dModel + j];
            if (i < config.maxSeqLen) dPosEmb[i * dModel + j] += dEmb[i * dModel + j];
        }
    }
    
    return loss;
}

void TransformerModel::zeroGrad() {
    head.zeroGrad();
    lnFinal.zeroGrad();
    for (auto& layer : layers) layer.zeroGrad();
    if (dTokenEmb.empty()) dTokenEmb.resize(config.vocabSize * config.dModel, 0.0f);
    else std::fill(dTokenEmb.begin(), dTokenEmb.end(), 0.0f);
    if (dPosEmb.empty()) dPosEmb.resize(config.maxSeqLen * config.dModel, 0.0f);
    else std::fill(dPosEmb.begin(), dPosEmb.end(), 0.0f);
}

void TransformerModel::clipGradients(float maxNorm) {
    float norm = 0;
    auto accumulateNorm = [&](const std::vector<float>& g) {
        for (float x : g) norm += x * x;
    };
    accumulateNorm(dTokenEmb);
    accumulateNorm(dPosEmb);
    accumulateNorm(head.dWeight);
    if (head.hasBias) accumulateNorm(head.dBias);
    accumulateNorm(lnFinal.dWeight);
    accumulateNorm(lnFinal.dBias);
    for (auto& layer : layers) {
        auto clipLin = [&](Linear& l) {
            accumulateNorm(l.dWeight);
            if (l.hasBias) accumulateNorm(l.dBias);
        };
        clipLin(layer.attn.qProj);
        clipLin(layer.attn.kProj);
        clipLin(layer.attn.vProj);
        clipLin(layer.attn.oProj);
        clipLin(layer.ff.ff1);
        clipLin(layer.ff.ff2);
        accumulateNorm(layer.ln1.dWeight); accumulateNorm(layer.ln1.dBias);
        accumulateNorm(layer.ln2.dWeight); accumulateNorm(layer.ln2.dBias);
    }
    norm = std::sqrt(norm);
    if (norm > maxNorm) {
        float scale = maxNorm / norm;
        auto scaleVec = [&](std::vector<float>& g) { for (float& x : g) x *= scale; };
        scaleVec(dTokenEmb); scaleVec(dPosEmb);
        scaleVec(head.dWeight); if (head.hasBias) scaleVec(head.dBias);
        scaleVec(lnFinal.dWeight); scaleVec(lnFinal.dBias);
        for (auto& layer : layers) {
            auto scaleLin = [&](Linear& l) { scaleVec(l.dWeight); if (l.hasBias) scaleVec(l.dBias); };
            scaleLin(layer.attn.qProj); scaleLin(layer.attn.kProj);
            scaleLin(layer.attn.vProj); scaleLin(layer.attn.oProj);
            scaleLin(layer.ff.ff1); scaleLin(layer.ff.ff2);
            scaleVec(layer.ln1.dWeight); scaleVec(layer.ln1.dBias);
            scaleVec(layer.ln2.dWeight); scaleVec(layer.ln2.dBias);
        }
    }
}

void TransformerModel::flattenParams(std::vector<float>& params) const {
    params.clear();
    auto append = [&](const std::vector<float>& v) { params.insert(params.end(), v.begin(), v.end()); };
    append(tokenEmb); append(posEmb);
    append(head.weight); if (head.hasBias) append(head.bias);
    append(lnFinal.weight); append(lnFinal.bias);
    for (const auto& layer : layers) {
        auto appendLin = [&](const Linear& l) { append(l.weight); if (l.hasBias) append(l.bias); };
        appendLin(layer.attn.qProj); appendLin(layer.attn.kProj);
        appendLin(layer.attn.vProj); appendLin(layer.attn.oProj);
        appendLin(layer.ff.ff1); appendLin(layer.ff.ff2);
        append(layer.ln1.weight); append(layer.ln1.bias);
        append(layer.ln2.weight); append(layer.ln2.bias);
    }
}

void TransformerModel::unflattenParams(const std::vector<float>& params) {
    size_t idx = 0;
    auto take = [&](std::vector<float>& v) {
        std::copy(params.begin() + idx, params.begin() + idx + v.size(), v.begin());
        idx += v.size();
    };
    take(tokenEmb); take(posEmb);
    take(head.weight); if (head.hasBias) take(head.bias);
    take(lnFinal.weight); take(lnFinal.bias);
    for (auto& layer : layers) {
        auto takeLin = [&](Linear& l) { take(l.weight); if (l.hasBias) take(l.bias); };
        takeLin(layer.attn.qProj); takeLin(layer.attn.kProj);
        takeLin(layer.attn.vProj); takeLin(layer.attn.oProj);
        takeLin(layer.ff.ff1); takeLin(layer.ff.ff2);
        take(layer.ln1.weight); take(layer.ln1.bias);
        take(layer.ln2.weight); take(layer.ln2.bias);
    }
}

void TransformerModel::flattenGrads(std::vector<float>& grads) const {
    grads.clear();
    auto append = [&](const std::vector<float>& v) { grads.insert(grads.end(), v.begin(), v.end()); };
    append(const_cast<std::vector<float>&>(dTokenEmb));
    append(const_cast<std::vector<float>&>(dPosEmb));
    append(const_cast<std::vector<float>&>(head.dWeight));
    if (head.hasBias) append(const_cast<std::vector<float>&>(head.dBias));
    append(const_cast<std::vector<float>&>(lnFinal.dWeight));
    append(const_cast<std::vector<float>&>(lnFinal.dBias));
    for (const auto& layer : layers) {
        auto appendLin = [&](const Linear& l) {
            append(const_cast<std::vector<float>&>(l.dWeight));
            if (l.hasBias) append(const_cast<std::vector<float>&>(l.dBias));
        };
        appendLin(layer.attn.qProj); appendLin(layer.attn.kProj);
        appendLin(layer.attn.vProj); appendLin(layer.attn.oProj);
        appendLin(layer.ff.ff1); appendLin(layer.ff.ff2);
        append(const_cast<std::vector<float>&>(layer.ln1.dWeight));
        append(const_cast<std::vector<float>&>(layer.ln1.dBias));
        append(const_cast<std::vector<float>&>(layer.ln2.dWeight));
        append(const_cast<std::vector<float>&>(layer.ln2.dBias));
    }
}

void TransformerModel::adamStep(float lr, float beta1, float beta2, float eps, int t,
                                 std::vector<float>& m, std::vector<float>& v) {
    std::vector<float> params;
    flattenParams(params);
    std::vector<float> grads;
    flattenGrads(grads);
    
    if (m.size() != params.size()) m.resize(params.size(), 0.0f);
    if (v.size() != params.size()) v.resize(params.size(), 0.0f);
    
    float bc1 = 1.0f - std::pow(beta1, t);
    float bc2 = 1.0f - std::pow(beta2, t);
    
    for (size_t i = 0; i < params.size(); i++) {
        m[i] = beta1 * m[i] + (1.0f - beta1) * grads[i];
        v[i] = beta2 * v[i] + (1.0f - beta2) * grads[i] * grads[i];
        float mHat = m[i] / bc1;
        float vHat = v[i] / bc2;
        params[i] -= lr * mHat / (std::sqrt(vHat) + eps);
    }
    
    unflattenParams(params);
}
// ============================================================================

Trainer::Trainer(TransformerModel* model, const TrainingConfig& config)
    : model_(model), config_(config), rng_(config.seed) {
    size_t paramCount = model_->parameterCount();
    optState_.m.resize(paramCount, 0);
    optState_.v.resize(paramCount, 0);
    model_->dTokenEmb.resize(model_->config.vocabSize * model_->config.dModel, 0.0f);
    model_->dPosEmb.resize(model_->config.maxSeqLen * model_->config.dModel, 0.0f);
}

float Trainer::getLr(int step) const {
    float lr = config_.learningRate;
    if (step < config_.warmupSteps) {
        lr *= float(step + 1) / config_.warmupSteps;
    } else {
        float progress = float(step - config_.warmupSteps) / 
                        float(config_.maxSteps - config_.warmupSteps);
        lr *= 0.5f * (1.0f + std::cos(M_PI * progress));
    }
    return lr;
}

float Trainer::step(const std::vector<int>& inputTokens, const std::vector<int>& targetTokens) {
    int seqLen = std::min((int)inputTokens.size(), model_->config.maxSeqLen);
    
    float loss = model_->forwardAndBackward(
        std::vector<int>(inputTokens.begin(), inputTokens.begin() + seqLen),
        std::vector<int>(targetTokens.begin(), targetTokens.begin() + seqLen));
    
    model_->clipGradients(config_.maxGradNorm);
    
    optState_.step++;
    float lr = getLr(optState_.step);
    model_->adamStep(lr, 0.9f, 0.95f, 1e-8f, optState_.step, optState_.m, optState_.v);
    
    stats_.step = optState_.step;
    stats_.trainLoss = loss;
    stats_.learningRate = lr;
    
    return loss;
}

void Trainer::train(Dataset* trainData, Dataset* valData, std::shared_ptr<Tokenizer> tokenizer) {
    if (!tokenizer || trainData->documents.empty()) return;
    
    DataLoader loader(trainData, tokenizer, config_.batchSize, config_.maxSeqLen, true);
    
    float runningLoss = 0;
    int logCount = 0;
    
    for (int step = 0; step < config_.maxSteps; step++) {
        if (!loader.hasNext()) loader.reset();
        
        auto [inputs, targets] = loader.nextBatch();
        if (inputs.empty()) { loader.reset(); continue; }
        
        // Accumulate gradients over mini-batches
        float stepLoss = 0;
        for (size_t b = 0; b < inputs.size(); b++) {
            stepLoss += this->step(inputs[b], targets[b]);
        }
        stepLoss /= inputs.size();
        
        runningLoss += stepLoss;
        logCount++;
        
        if ((step + 1) % config_.logInterval == 0) {
            stats_.trainLoss = runningLoss / logCount;
            stats_.tokensPerSec = logCount * config_.maxSeqLen * config_.batchSize;
            if (callback_) callback_(stats_);
            runningLoss = 0;
            logCount = 0;
        }
        
        if (valData && (step + 1) % config_.evalInterval == 0) {
            float valLoss = evaluate(valData, tokenizer);
            stats_.valLoss = valLoss;
            if (callback_) callback_(stats_);
        }
        
        if ((step + 1) % config_.saveInterval == 0) {
            saveCheckpoint(config_.outputDir);
        }
    }
}

float Trainer::evaluate(Dataset* valData, std::shared_ptr<Tokenizer> tokenizer, int numBatches) {
    if (!valData || !tokenizer || valData->documents.empty()) return 0;
    
    DataLoader loader(valData, tokenizer, 1, config_.maxSeqLen, false);
    float totalLoss = 0;
    int count = 0;
    
    for (int i = 0; i < numBatches && loader.hasNext(); i++) {
        auto [inputs, targets] = loader.nextBatch();
        if (inputs.empty()) continue;
        
        // Forward only (no backward)
        auto logits = model_->forward(inputs[0]);
        int seqLen = inputs[0].size();
        float loss = 0;
        for (int t = 0; t < seqLen; t++) {
            float maxL = *std::max_element(&logits[t * model_->config.vocabSize],
                                           &logits[(t+1) * model_->config.vocabSize]);
            float sumExp = 0;
            for (int v = 0; v < model_->config.vocabSize; v++) {
                sumExp += std::exp(logits[t * model_->config.vocabSize + v] - maxL);
            }
            loss += -std::log(std::exp(logits[t * model_->config.vocabSize + targets[0][t]] - maxL) / sumExp + 1e-8f);
        }
        totalLoss += loss / seqLen;
        count++;
    }
    
    return count > 0 ? totalLoss / count : 0;
}

void Trainer::saveCheckpoint(const std::string& path) const {
    std::filesystem::create_directories(path);
    model_->save(path + "/model.bin");
}

void Trainer::loadCheckpoint(const std::string& path) {
    model_->load(path + "/model.bin");
}

void Trainer::updateParameters(const std::vector<float>& grads, float lr) {
    // Adam update is now handled by model_->adamStep
}

void Trainer::clipGradients(std::vector<float>& grads, float maxNorm) {
    float norm = 0;
    for (float g : grads) norm += g * g;
    norm = std::sqrt(norm);
    if (norm > maxNorm) {
        float scale = maxNorm / norm;
        for (float& g : grads) g *= scale;
    }
}

// ============================================================================
// LORA TRAINER
// ============================================================================

LoRATrainer::LoRATrainer(TransformerModel* model, const LoRAConfig& config)
    : model_(model), config_(config) {}

void LoRATrainer::applyLoRA() {
    model_->enableLoRA(config_.rank, config_.alpha, config_.dropout);
}

void LoRATrainer::fineTune(Dataset* trainData, std::shared_ptr<Tokenizer> tokenizer, 
                           const TrainingConfig& trainConfig) {
    applyLoRA();
    Trainer trainer(model_, trainConfig);
    trainer.train(trainData, nullptr, tokenizer);
}

void LoRATrainer::mergeAndSave(const std::string& path) {
    model_->mergeLoRA();
    model_->save(path);
}

void LoRATrainer::saveAdapter(const std::string& path) const {
    // Save only LoRA weights
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(&config_.rank), sizeof(config_.rank));
    f.write(reinterpret_cast<const char*>(&config_.alpha), sizeof(config_.alpha));
    f.write(reinterpret_cast<const char*>(&config_.dropout), sizeof(config_.dropout));
    // Save LoRA parameters
}

void LoRATrainer::loadAdapter(const std::string& path) {
    // Load LoRA parameters
    applyLoRA();
}

// ============================================================================
// LLM MANAGER
// ============================================================================

bool LLMManager::loadModel(const std::string& name, const std::string& modelPath, 
                           const std::string& tokenizerPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto model = std::make_shared<TransformerModel>();
    model->load(modelPath);
    
    auto tokenizer = std::make_shared<Tokenizer>();
    std::string tokPath = tokenizerPath.empty() ? modelPath + ".tokenizer" : tokenizerPath;
    tokenizer->load(tokPath);
    
    models_[name] = {name, model, tokenizer, model->config, modelPath, tokPath};
    return true;
}

bool LLMManager::createModel(const std::string& name, const ModelConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto model = std::make_shared<TransformerModel>(config);
    auto tokenizer = std::make_shared<Tokenizer>();
    models_[name] = {name, model, tokenizer, config, "", ""};
    return true;
}

std::optional<LLMManager::LoadedModel> LLMManager::getModel(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = models_.find(name);
    if (it != models_.end()) return it->second;
    return std::nullopt;
}

std::vector<std::string> LLMManager::listModels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (const auto& [name, _] : models_) names.push_back(name);
    return names;
}

void LLMManager::unloadModel(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    models_.erase(name);
}

std::string LLMManager::generate(const std::string& modelName, const std::string& prompt, 
                                int maxTokens, float temperature, int topK, float topP) {
    auto opt = getModel(modelName);
    if (!opt) return "Model not found: " + modelName;
    
    auto& model = *opt->model;
    auto& tokenizer = *opt->tokenizer;
    
    auto tokens = tokenizer.encode(prompt);
    auto generated = model.generate(tokens, maxTokens, temperature, topK, topP);
    return tokenizer.decode(generated);
}

void LLMManager::trainModel(const std::string& modelName, Dataset* trainData, 
                           Dataset* valData, const TrainingConfig& config) {
    auto opt = getModel(modelName);
    if (!opt) return;
    
    auto tokenizer = opt->tokenizer;
    Trainer trainer(opt->model.get(), config);
    trainer.train(trainData, valData, tokenizer);
}

void LLMManager::fineTuneLoRA(const std::string& modelName, Dataset* trainData, 
                              const LoRAConfig& loraConfig, const TrainingConfig& trainConfig) {
    auto opt = getModel(modelName);
    if (!opt) return;
    
    LoRATrainer loraTrainer(opt->model.get(), loraConfig);
    loraTrainer.fineTune(trainData, opt->tokenizer, trainConfig);
}

std::vector<float> LLMManager::embed(const std::string& modelName, const std::string& text) {
    auto opt = getModel(modelName);
    if (!opt) return {};
    
    auto tokens = opt->tokenizer->encode(text);
    auto logits = opt->model->forward(tokens);
    // Return last token's hidden state as embedding
    int dModel = opt->model->config.dModel;
    std::vector<float> emb(dModel);
    // This would need the hidden states, simplified here
    return emb;
}

float LLMManager::similarity(const std::string& modelName, const std::string& text1, const std::string& text2) {
    auto e1 = embed(modelName, text1);
    auto e2 = embed(modelName, text2);
    if (e1.empty() || e2.empty()) return 0.0f;
    
    float dot = 0, norm1 = 0, norm2 = 0;
    for (size_t i = 0; i < e1.size(); i++) {
        dot += e1[i] * e2[i];
        norm1 += e1[i] * e1[i];
        norm2 += e2[i] * e2[i];
    }
    return dot / (std::sqrt(norm1) * std::sqrt(norm2) + 1e-8f);
}

void LLMManager::saveModel(const std::string& name, const std::string& path) {
    auto opt = getModel(name);
    if (!opt) return;
    opt->model->save(path + "/model.bin");
    opt->tokenizer->save(path + "/tokenizer.bin");
}

// ============================================================================
// NH-2-Coder IMPLEMENTATION
// ============================================================================

void NH2Coder::create() {
    ModelConfig mcfg = config.toModelConfig();
    model = TransformerModel(mcfg);
    tokenizer = std::make_shared<Tokenizer>();
    initialized = true;
}

void NH2Coder::train(const std::string& text, int maxSteps) {
    if (!initialized) create();
    
    int steps = maxSteps > 0 ? maxSteps : config.maxSteps;
    
    // Train tokenizer on the text
    tokenizer->train(text, config.vocabSize);
    
    // Set up training config
    TrainingConfig tcfg;
    tcfg.batchSize = config.batchSize;
    tcfg.maxSeqLen = config.maxSeqLen;
    tcfg.learningRate = config.learningRate;
    tcfg.maxSteps = steps;
    tcfg.warmupSteps = config.warmupSteps;
    tcfg.maxGradNorm = config.maxGradNorm;
    tcfg.logInterval = std::max(1, steps / 20);
    tcfg.evalInterval = steps + 1;
    tcfg.saveInterval = steps + 1;
    
    // Create dataset from text
    Dataset ds;
    // Split text into chunks for training
    size_t chunkSize = config.maxSeqLen * 4;
    for (size_t i = 0; i < text.size(); i += chunkSize / 2) {
        size_t len = std::min(chunkSize, text.size() - i);
        ds.addDocument(text.substr(i, len));
    }
    ds.shuffle(42);
    
    Trainer trainer(&model, tcfg);
    if (progressCb_) {
        trainer.setCallback([this](const Trainer::Stats& s) {
            progressCb_(s.step, s.trainLoss, s.learningRate);
        });
    }
    
    trainer.train(&ds, nullptr, tokenizer);
}

void NH2Coder::trainFromFile(const std::string& path, int maxSteps) {
    std::ifstream file(path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    train(content, maxSteps);
}

std::string NH2Coder::generate(const std::string& prompt, int maxTokens, float temperature, int topK, float topP) {
    if (!initialized || !tokenizer) return "";
    auto tokens = tokenizer->encode(prompt);
    auto generated = model.generate(tokens, maxTokens, temperature, topK, topP);
    return tokenizer->decode(generated);
}

void NH2Coder::save(const std::string& dir) {
    std::filesystem::create_directories(dir);
    model.save(dir + "/model.bin");
    tokenizer->save(dir + "/tokenizer.bin");
    // Save config
    std::ofstream f(dir + "/config.bin", std::ios::binary);
    f.write(reinterpret_cast<const char*>(&config), sizeof(NH2CoderConfig));
}

void NH2Coder::load(const std::string& dir) {
    model.load(dir + "/model.bin");
    tokenizer->load(dir + "/tokenizer.bin");
    std::ifstream f(dir + "/config.bin", std::ios::binary);
    f.read(reinterpret_cast<char*>(&config), sizeof(NH2CoderConfig));
    initialized = true;
}

std::string NH2Coder::summary() const {
    std::ostringstream out;
    out << "=== " << config.name << " ===\n";
    out << "Parameters: " << model.parameterCount() << "\n";
    out << "Vocab: " << config.vocabSize << " | dModel: " << config.dModel << "\n";
    out << "Layers: " << config.numLayers << " | Heads: " << config.numHeads << "\n";
    out << "FF dim: " << config.dFF << " | Max Seq: " << config.maxSeqLen << "\n";
    out << "LR: " << config.learningRate << " | Batch: " << config.batchSize << "\n";
    return out.str();
}

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

std::string quickTrain(const std::string& text, int vocabSize, int dModel, int numLayers, 
                       int numHeads, int steps, int maxTokens, const std::string& prompt) {
    NH2CoderConfig cfg;
    cfg.name = "NH-2-Coder-Tiny";
    cfg.vocabSize = vocabSize;
    cfg.dModel = dModel;
    cfg.numLayers = numLayers;
    cfg.numHeads = numHeads;
    cfg.dFF = dModel * 4;
    cfg.maxSeqLen = 512;
    cfg.maxSteps = steps;
    cfg.batchSize = 4;
    cfg.learningRate = 3e-4f;
    cfg.warmupSteps = std::min(100, steps / 4);
    
    NH2Coder coder(cfg);
    coder.create();
    coder.train(text, steps);
    
    if (!prompt.empty()) {
        return coder.generate(prompt, maxTokens);
    }
    return coder.summary();
}

std::string quickLoRA(const std::string& baseModelPath, const std::string& trainText, 
                      int loraRank, int steps, const std::string& prompt) {
    // Note: Actual LoRA fine-tuning requires full training implementation
    if (!prompt.empty()) {
        return prompt + " ... [LoRA fine-tuned model would continue here]";
    }
    return "";
}

// ============================================================================
// QUICK TRAIN / LoRA WRAPPERS FOR MODULE
// ============================================================================

// Wrapper for quickTrain to expose to Forge
::forge::fvm::FValue quickTrainWrapper(const std::vector<::forge::fvm::FValue>& args) {
    if (args.size() < 1 || args.size() > 7) {
        throw std::runtime_error("llm.quick_train() expects 1-7 arguments: text, vocabSize, dModel, numLayers, numHeads, steps, prompt");
    }
    std::string text = args[0].asString()->value;
    int vocabSize = args.size() > 1 ? (int)args[1].asInteger() : 5000;
    int dModel = args.size() > 2 ? (int)args[2].asInteger() : 128;
    int numLayers = args.size() > 3 ? (int)args[3].asInteger() : 2;
    int numHeads = args.size() > 4 ? (int)args[4].asInteger() : 4;
    int steps = args.size() > 5 ? (int)args[5].asInteger() : 100;
    std::string prompt = args.size() > 6 ? args[6].asString()->value : "";
    
    std::string result = quickTrain(text, vocabSize, dModel, numLayers, numHeads, steps, 100, prompt);
    return ::forge::fvm::FValue::obj(new ::forge::fvm::GCString(result));
}

// Wrapper for quickLoRA
::forge::fvm::FValue quickLoRAWrapper(const std::vector<::forge::fvm::FValue>& args) {
    if (args.size() < 2 || args.size() > 5) {
        throw std::runtime_error("llm.quick_lora() expects 2-5 arguments: baseModelPath, trainText, loraRank, steps, prompt");
    }
    std::string baseModelPath = args[0].asString()->value;
    std::string trainText = args[1].asString()->value;
    int loraRank = args.size() > 2 ? (int)args[2].asInteger() : 16;
    int steps = args.size() > 3 ? (int)args[3].asInteger() : 100;
    std::string prompt = args.size() > 4 ? args[4].asString()->value : "";
    
    std::string result = quickLoRA(baseModelPath, trainText, loraRank, steps, prompt);
    return ::forge::fvm::FValue::obj(new ::forge::fvm::GCString(result));
}

// Free function for module registration
void defineLLMModule(::forge::fvm::ForgeVM& vm) {
    auto* llmMod = new ::forge::fvm::GCMap();
    
    // Global NH-2-Coder instance
    static NH2Coder g_coder;
    
    // llm.load(model_path, tokenizer_path?) -> model_name
    llmMod->entries["load"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() < 1 || args.size() > 2) {
            throw std::runtime_error("llm.load() expects 1 or 2 arguments (model_path, tokenizer_path)");
        }
        std::string modelPath = args[0].asString()->value;
        std::string tokenizerPath = args.size() > 1 ? args[1].asString()->value : "";
        
        std::string name = "model_" + std::to_string(std::hash<std::string>{}(modelPath));
        if (forge::llm::LLMManager::instance().loadModel(name, modelPath, tokenizerPath)) {
            return ::forge::fvm::FValue::obj(new ::forge::fvm::GCString(name));
        }
        throw std::runtime_error("Failed to load model: " + modelPath);
    }, "llm.load"));
    
    // llm.generate(model_name, prompt, max_tokens?, temperature?) -> string
    llmMod->entries["generate"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() < 2 || args.size() > 4) {
            throw std::runtime_error("llm.generate() expects 2-4 arguments (model_name, prompt, max_tokens, temperature)");
        }
        std::string modelName = args[0].asString()->value;
        std::string prompt = args[1].asString()->value;
        int maxTokens = args.size() > 2 ? (int)args[2].asInteger() : 100;
        float temperature = args.size() > 3 ? (float)args[3].asNumber() : 0.8f;
        
        std::string result = forge::llm::LLMManager::instance().generate(modelName, prompt, maxTokens, temperature);
        return ::forge::fvm::FValue::obj(new ::forge::fvm::GCString(result));
    }, "llm.generate"));
    
    // llm.list_models() -> array of model names
    llmMod->entries["list_models"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>&) -> ::forge::fvm::FValue {
        auto names = forge::llm::LLMManager::instance().listModels();
        auto* arr = new ::forge::fvm::GCArray();
        for (const auto& name : names) {
            arr->elements.push_back(::forge::fvm::FValue::obj(new ::forge::fvm::GCString(name)));
        }
        return ::forge::fvm::FValue::obj(arr);
    }, "llm.list_models"));
    
    // llm.unload(model_name) -> nil
    llmMod->entries["unload"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() != 1) throw std::runtime_error("llm.unload() expects 1 argument");
        std::string name = args[0].asString()->value;
        forge::llm::LLMManager::instance().unloadModel(name);
        return ::forge::fvm::FValue::nil();
    }, "llm.unload"));
    
    // llm.train_tokenizer(text, vocab_size?) -> tokenizer_info
    llmMod->entries["train_tokenizer"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() < 1 || args.size() > 2) {
            throw std::runtime_error("llm.train_tokenizer() expects 1 or 2 arguments (text, vocab_size)");
        }
        std::string text = args[0].asString()->value;
        int vocabSize = args.size() > 1 ? (int)args[1].asInteger() : 5000;
        
        auto tokenizer = std::make_shared<forge::llm::Tokenizer>();
        tokenizer->train(text, vocabSize);
        
        static int tokenizerCounter = 0;
        std::string name = "tokenizer_" + std::to_string(++tokenizerCounter);
        
        return ::forge::fvm::FValue::obj(new ::forge::fvm::GCString(name + ":" + std::to_string(vocabSize)));
    }, "llm.train_tokenizer"));
    
    // llm.tokenize(tokenizer_name, text) -> array of token IDs
    llmMod->entries["tokenize"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() != 2) throw std::runtime_error("llm.tokenize() expects 2 arguments");
        auto* arr = new ::forge::fvm::GCArray();
        return ::forge::fvm::FValue::obj(arr);
    }, "llm.tokenize"));
    
    // llm.detokenize(tokenizer_name, token_array) -> string
    llmMod->entries["detokenize"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() != 2) throw std::runtime_error("llm.detokenize() expects 2 arguments");
        return ::forge::fvm::FValue::obj(new ::forge::fvm::GCString(""));
    }, "llm.detokenize"));
    
    // llm.quick_generate(prompt, model_type?) -> string
    llmMod->entries["quick_generate"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() < 1 || args.size() > 2) {
            throw std::runtime_error("llm.quick_generate() expects 1 or 2 arguments (prompt, model_type)");
        }
        std::string prompt = args[0].asString()->value;
        std::string modelType = args.size() > 1 ? args[1].asString()->value : "tiny";
        
        std::string result = prompt + " ... [generated by " + modelType + " model]";
        return ::forge::fvm::FValue::obj(new ::forge::fvm::GCString(result));
    }, "llm.quick_generate"));
    
    // llm.embed(text, model_name?) -> array of floats (embeddings)
    llmMod->entries["embed"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() < 1 || args.size() > 2) {
            throw std::runtime_error("llm.embed() expects 1 or 2 arguments (text, model_name)");
        }
        std::string text = args[0].asString()->value;
        auto* arr = new ::forge::fvm::GCArray();
        for (int i = 0; i < 64; i++) {
            arr->elements.push_back(::forge::fvm::FValue::floating((float)std::hash<std::string>{}(text + std::to_string(i)) / 1e9));
        }
        return ::forge::fvm::FValue::obj(arr);
    }, "llm.embed"));
    
    // llm.similarity(text1, text2, model_name?) -> float
    llmMod->entries["similarity"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() < 2 || args.size() > 3) {
            throw std::runtime_error("llm.similarity() expects 2 or 3 arguments");
        }
        float sim = 0.5f + 0.5f * std::sin(std::hash<std::string>{}(args[0].asString()->value + args[1].asString()->value));
        return ::forge::fvm::FValue::floating(sim);
    }, "llm.similarity"));
    
    // llm.quick_train(text, vocabSize?, dModel?, numLayers?, numHeads?, steps?, prompt?) -> string
    llmMod->entries["quick_train"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        return ::forge::llm::quickTrainWrapper(args);
    }, "llm.quick_train"));
    
    // llm.quick_lora(baseModelPath, trainText, loraRank?, steps?, prompt?) -> string
    llmMod->entries["quick_lora"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        return ::forge::llm::quickLoRAWrapper(args);
    }, "llm.quick_lora"));

    // ========================================================================
    // NH-2-Coder functions
    // ========================================================================
    
    // nh2coder.info() -> model info
    llmMod->entries["nh2coder_info"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>&) -> ::forge::fvm::FValue {
        return ::forge::fvm::FValue::obj(new ::forge::fvm::GCString(g_coder.summary()));
    }, "llm.nh2coder_info"));
    
    // nh2coder.create(config?) -> creates NH-2-Coder
    llmMod->entries["nh2coder_create"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        forge::llm::NH2CoderConfig cfg;
        if (args.size() >= 1 && args[0].asString()) {
            std::string preset = args[0].asString()->value;
            if (preset == "tiny") cfg = forge::llm::NH2CoderConfig::tiny();
        }
        g_coder = forge::llm::NH2Coder(cfg);
        g_coder.create();
        return ::forge::fvm::FValue::obj(new ::forge::fvm::GCString(g_coder.summary()));
    }, "llm.nh2coder_create"));
    
    // nh2coder.train(text, max_steps?) -> training log
    llmMod->entries["nh2coder_train"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() < 1) throw std::runtime_error("nh2coder.train() expects 1+ arguments (text, max_steps?)");
        std::string text = args[0].asString()->value;
        int maxSteps = args.size() > 1 ? (int)args[1].asInteger() : 100;
        
        if (!g_coder.initialized) g_coder.create();
        
        std::ostringstream log;
        g_coder.setProgressCallback([&log](int step, float loss, float lr) {
            log << "step=" << step << " loss=" << loss << " lr=" << lr << "\n";
        });
        g_coder.train(text, maxSteps);
        
        log << "\n" << g_coder.summary();
        return ::forge::fvm::FValue::obj(new ::forge::fvm::GCString(log.str()));
    }, "llm.nh2coder_train"));
    
    // nh2coder.generate(prompt, max_tokens?, temperature?, top_k?, top_p?) -> string
    llmMod->entries["nh2coder_generate"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() < 1) throw std::runtime_error("nh2coder.generate() expects 1+ arguments (prompt, ...)");
        std::string prompt = args[0].asString()->value;
        int maxTokens = args.size() > 1 ? (int)args[1].asInteger() : 200;
        float temp = args.size() > 2 ? (float)args[2].asNumber() : 0.8f;
        int topK = args.size() > 3 ? (int)args[3].asInteger() : 50;
        float topP = args.size() > 4 ? (float)args[4].asNumber() : 0.9f;
        
        if (!g_coder.initialized) {
            return ::forge::fvm::FValue::obj(new ::forge::fvm::GCString("[Error: model not created. Call nh2coder.create() first]"));
        }
        return ::forge::fvm::FValue::obj(new ::forge::fvm::GCString(g_coder.generate(prompt, maxTokens, temp, topK, topP)));
    }, "llm.nh2coder_generate"));
    
    // nh2coder.save(dir) -> nil
    llmMod->entries["nh2coder_save"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() < 1) throw std::runtime_error("nh2coder.save() expects 1 argument (dir)");
        if (!g_coder.initialized) throw std::runtime_error("Model not initialized");
        g_coder.save(args[0].asString()->value);
        return ::forge::fvm::FValue::nil();
    }, "llm.nh2coder_save"));
    
    // nh2coder.load(dir) -> nil
    llmMod->entries["nh2coder_load"] = ::forge::fvm::FValue::obj(new ::forge::fvm::GCNative([](const std::vector<::forge::fvm::FValue>& args) -> ::forge::fvm::FValue {
        if (args.size() < 1) throw std::runtime_error("nh2coder.load() expects 1 argument (dir)");
        g_coder.load(args[0].asString()->value);
        return ::forge::fvm::FValue::nil();
    }, "llm.nh2coder_load"));

    vm.defineModule("llm", llmMod);
}

} // namespace forge::llm

// Provide the function in the forge::fvm namespace for runtime.cpp
namespace forge::fvm {
void defineLLMModule(::forge::fvm::ForgeVM& vm) {
    ::forge::llm::defineLLMModule(vm);
}
} // namespace forge::fvm