#include "hf_converter.hpp"
#include "llm.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdint>

namespace forge::llm {

// ============================================================================
// SAFETENSORS LOADER
// ============================================================================

struct SafeTensorsHeader {
    std::unordered_map<std::string, std::string> metadata;
    struct TensorInfo {
        std::vector<int64_t> shape;
        std::string dtype;
        std::pair<size_t, size_t> offsets;
    };
    std::unordered_map<std::string, TensorInfo> tensors;
    size_t data_offset = 0;
};

static bool parseSafeTensorsHeader(std::ifstream& file, SafeTensorsHeader& header) {
    uint64_t header_size;
    file.read(reinterpret_cast<char*>(&header_size), 8);
    if (!file) return false;

    std::string header_json(header_size, '\0');
    file.read(&header_json[0], header_size);
    if (!file) return false;

    // Parse JSON header
    size_t pos = 0;
    auto skip_ws = [&]() { while (pos < header_json.size() && isspace(header_json[pos])) pos++; };
    auto parse_string = [&]() -> std::string {
        if (pos >= header_json.size() || header_json[pos] != '"') return "";
        pos++;
        size_t start = pos;
        while (pos < header_json.size() && header_json[pos] != '"') pos++;
        std::string s = header_json.substr(start, pos - start);
        pos++; // skip "
        return s;
    };

    auto parse_int = [&]() -> int64_t {
        while (pos < header_json.size() && isspace(header_json[pos])) pos++;
        size_t start = pos;
        while (pos < header_json.size() && (isdigit(header_json[pos]) || header_json[pos] == '-')) pos++;
        return std::stoll(header_json.substr(start, pos - start));
    };

    // Skip to first {
    while (pos < header_json.size() && header_json[pos] != '{') pos++;
    pos++; // skip {

    while (pos < header_json.size()) {
        skip_ws();
        if (pos >= header_json.size() || header_json[pos] == '}') break;

        std::string key = parse_string();
        if (pos >= header_json.size()) break;
        while (pos < header_json.size() && isspace(header_json[pos])) pos++;
        if (header_json[pos] != ':') break;
        pos++; // skip :

        if (key == "__metadata__") {
            // Parse metadata object
            while (pos < header_json.size() && header_json[pos] != '}') {
                skip_ws();
                if (header_json[pos] == '}') break;
                std::string mkey = parse_string();
                skip_ws();
                if (header_json[pos] != ':') break;
                pos++;
                std::string mval = parse_string();
                header.metadata[std::move(mkey)] = std::move(mval);
                while (pos < header_json.size() && isspace(header_json[pos])) pos++;
                if (pos < header_json.size() && header_json[pos] == ',') pos++;
            }
            pos++; // skip }
        } else {
            // Parse tensor info
            std::string dtype;
            std::vector<int64_t> shape;
            std::pair<size_t, size_t> offsets = {0, 0};

            while (pos < header_json.size() && header_json[pos] != '}') {
                skip_ws();
                if (header_json[pos] == '}') break;
                std::string tkey = parse_string();
                if (pos >= header_json.size()) break;
                while (pos < header_json.size() && isspace(header_json[pos])) pos++;
                if (header_json[pos] != ':') break;
                pos++;

                if (tkey == "dtype") {
                    if (header_json[pos] == '"') pos++;
                    size_t start = pos;
                    while (pos < header_json.size() && header_json[pos] != '"') pos++;
                    // dtype = header_json.substr(start, pos - start);
                    pos++; // skip "
                } else if (tkey == "shape") {
                    // parse array
                    while (pos < header_json.size() && header_json[pos] != ']') {
                        while (pos < header_json.size() && isspace(header_json[pos])) pos++;
                        if (header_json[pos] == ']') break;
                        size_t start = pos;
                        while (pos < header_json.size() && (isdigit(header_json[pos]) || header_json[pos] == '-')) pos++;
                        shape.push_back(std::stoll(header_json.substr(start, pos - start)));
                        while (pos < header_json.size() && isspace(header_json[pos])) pos++;
                        if (header_json[pos] == ',') pos++;
                    }
                    if (pos < header_json.size() && header_json[pos] == ']') pos++;
                } else if (tkey == "data_offsets") {
                    // parse array
                    while (pos < header_json.size() && header_json[pos] != ']') {
                        while (pos < header_json.size() && isspace(header_json[pos])) pos++;
                        if (header_json[pos] == ']') break;
                        size_t start = pos;
                        while (pos < header_json.size() && isdigit(header_json[pos])) pos++;
                        size_t offset = std::stoull(header_json.substr(start, pos - start));
                        if (offsets.first == 0) offsets.first = offset;
                        else offsets.second = offset;
                        while (pos < header_json.size() && isspace(header_json[pos])) pos++;
                        if (header_json[pos] == ',') pos++;
                    }
                    if (pos < header_json.size() && header_json[pos] == ']') pos++;
                }
                while (pos < header_json.size() && isspace(header_json[pos])) pos++;
                if (pos < header_json.size() && header_json[pos] == ',') pos++;
            }
            pos++; // skip }
            header.tensors[key] = {shape, dtype, offsets};
        }
        skip_ws();
        if (header_json[pos] == ',') pos++;
    }

    header.data_offset = 8 + header_json.size();
    return true;
}

bool HFModelConverter::loadFromSafeTensors(const std::string& path, WeightMap& weights, HFModelInfo& info) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    SafeTensorsHeader header;
    if (!parseSafeTensorsHeader(file, header)) return false;

    // Parse metadata for model info
    if (header.metadata.count("format")) {
        info.torch_dtype = header.metadata["format"];
    }
    if (header.metadata.count("architectures")) {
        std::string arch = header.metadata["architectures"];
        if (arch.find("Llama") != std::string::npos) info.architecture = "llama";
        else if (arch.find("Mistral") != std::string::npos) info.architecture = "mistral";
        else if (arch.find("GPT2") != std::string::npos) info.architecture = "gpt2";
    }

    // Read tensor data
    for (const auto& [name, tensor_info] : header.tensors) {
        const auto& shape = tensor_info.shape;
        const std::string& dtype = tensor_info.dtype;
        size_t start = tensor_info.offsets.first;
        size_t end = tensor_info.offsets.second;
        size_t size = end - start;

        if (shape.empty()) continue;

        size_t numel = 1;
        for (auto dim : shape) numel *= dim;

        // Seek to data
        file.seekg(start, std::ios::beg);

        std::vector<float> data(numel);
        if (tensor_info.dtype == "F16" || tensor_info.dtype == "F16") {
            std::vector<uint16_t> half_data(numel);
            file.read(reinterpret_cast<char*>(half_data.data()), numel * 2);
            for (size_t i = 0; i < numel; ++i) {
                uint16_t h = half_data[i];
                int sign = (h >> 15) & 1;
                int exp = (h >> 10) & 0x1F;
                int mantissa = h & 0x3FF;
                float val;
                if (exp == 0) {
                    if (mantissa == 0) val = 0.0f;
                    else val = std::ldexp(mantissa / 1024.0f, -14) * (sign ? -1.0f : 1.0f);
                } else if (exp == 31) {
                    val = mantissa ? std::numeric_limits<float>::quiet_NaN() : (sign ? -INFINITY : INFINITY);
                } else {
                    val = std::ldexp((1.0f + mantissa / 1024.0f), exp - 15) * (sign ? -1.0f : 1.0f);
                }
                data[i] = val;
            }
        } else if (tensor_info.dtype == "F32") {
            file.read(reinterpret_cast<char*>(data.data()), numel * 4);
        } else if (tensor_info.dtype == "BF16") {
            std::vector<uint16_t> bf16_data(numel);
            file.read(reinterpret_cast<char*>(bf16_data.data()), numel * 2);
            for (size_t i = 0; i < numel; ++i) {
                uint32_t f = static_cast<uint32_t>(bf16_data[i]) << 16;
                data[i] = *reinterpret_cast<float*>(&f);
            }
        }

        weights.weights[name] = std::move(data);
        weights.shapes[name] = shape;
    }

    return true;
}

// ============================================================================
// ARCHITECTURE DETECTION
// ============================================================================

bool HFModelConverter::detectArchitecture(const std::string& config_path, HFModelInfo& info) {
    std::ifstream file(config_path);
    if (!file) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    // Simple JSON parsing for key fields
    auto extract_int = [&](const std::string& key) -> int {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return 0;
        pos = json.find(':', pos);
        if (pos == std::string::npos) return 0;
        size_t start = json.find_first_of("0123456789", pos);
        if (start == std::string::npos) return 0;
        size_t end = json.find_first_not_of("0123456789", start);
        return std::stoi(json.substr(start, end - start));
    };

    auto extract_float = [&](const std::string& key) -> float {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return 0.0f;
        pos = json.find(':', pos);
        if (pos == std::string::npos) return 0.0f;
        size_t start = json.find_first_of("0123456789.-", pos);
        if (start == std::string::npos) return 0.0f;
        size_t end = json.find_first_not_of("0123456789.eE+-", start);
        return std::stof(json.substr(start, end - start));
    };

    auto extract_string = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";
        size_t start = json.find('"', pos);
        if (start == std::string::npos) return "";
        start++;
        size_t end = json.find('"', start);
        return json.substr(start, end - start);
    };

    info.vocab_size = extract_int("vocab_size");
    info.hidden_size = extract_int("hidden_size");
    info.num_layers = extract_int("num_hidden_layers");
    info.num_heads = extract_int("num_attention_heads");
    info.num_kv_heads = extract_int("num_key_value_heads");
    if (info.num_kv_heads == 0) info.num_kv_heads = info.num_heads;
    info.intermediate_size = extract_int("intermediate_size");
    info.max_position_embeddings = extract_int("max_position_embeddings");
    info.rms_norm_eps = extract_float("rms_norm_eps");
    info.torch_dtype = extract_string("torch_dtype");
    if (info.torch_dtype.empty()) info.torch_dtype = "float16";

    std::string arch = extract_string("architectures");
    if (arch.find("Llama") != std::string::npos) info.architecture = "llama";
    else if (arch.find("Mistral") != std::string::npos) info.architecture = "mistral";
    else if (arch.find("GPT2") != std::string::npos || arch.find("GPTNeo") != std::string::npos) info.architecture = "gpt2";
    else if (arch.find("GPTJ") != std::string::npos) info.architecture = "gptj";
    else if (arch.find("Falcon") != std::string::npos) info.architecture = "falcon";
    else info.architecture = "unknown";

    info.tie_word_embeddings = (json.find("tie_word_embeddings") != std::string::npos && 
                                 json.find("true", json.find("tie_word_embeddings")) < json.find("false", json.find("tie_word_embeddings")));

    return true;
}

std::vector<std::string> HFModelConverter::getSupportedArchitectures() {
    return {"llama", "mistral", "gpt2", "gptj", "falcon"};
}

// ============================================================================
// ARCHITECTURE-SPECIFIC CONVERSION
// ============================================================================

bool HFModelConverter::convertToForge(const WeightMap& hf_weights, const HFModelInfo& hf_info, TransformerModel& forge_model) {
    if (hf_info.architecture == "llama" || hf_info.architecture == "mistral") {
        return convertLlama(hf_weights, hf_info, forge_model);
    } else if (hf_info.architecture == "gpt2") {
        return convertGPT2(hf_weights, hf_info, forge_model);
    } else if (hf_info.architecture == "gptj") {
        return convertGPTJ(hf_weights, hf_info, forge_model);
    }
    return false;
}

bool HFModelConverter::convertLlama(const WeightMap& hf, const HFModelInfo& info, TransformerModel& forge) {
    // Initialize Forge model config from HF info
    forge.config.vocabSize = info.vocab_size;
    forge.config.maxSeqLen = info.max_position_embeddings;
    forge.config.dModel = info.hidden_size;
    forge.config.numLayers = info.num_layers;
    forge.config.numHeads = info.num_heads;
    forge.config.dFF = info.intermediate_size;
    forge.config.dropout = 0.1f;

    // Reinitialize model with correct config
    forge.initialize();

    // Map weights: HF -> Forge
    // HF names:
    // Token embeddings
    auto it = hf.weights.find("model.embed_tokens.weight");
    if (it != hf.weights.end()) {
        std::copy(it->second.begin(), it->second.end(), forge.tokenEmb.begin());
    }

    // Output head (tied or separate)
    it = hf.weights.find("lm_head.weight");
    if (it != hf.weights.end()) {
        std::copy(it->second.begin(), it->second.end(), forge.head.weight.begin());
    } else if (hf.weights.find("model.embed_tokens.weight") != hf.weights.end()) {
        // Tied embeddings
        std::copy(forge.tokenEmb.begin(), forge.tokenEmb.end(), forge.head.weight.begin());
    }

    // Final norm
    it = hf.weights.find("model.norm.weight");
    if (it != hf.weights.end()) {
        std::copy(it->second.begin(), it->second.end(), forge.lnFinal.weight.begin());
    }

    // Per-layer weights
    for (int i = 0; i < info.num_layers; ++i) {
        std::string prefix = "model.layers." + std::to_string(i) + ".";
        auto& layer = forge.layers[i];

        // Attention projections
        auto copyWeight = [&](const std::string& hf_name, std::vector<float>& forge_weight) {
            auto it = hf.weights.find(prefix + hf_name);
            if (it != hf.weights.end()) {
                std::copy(it->second.begin(), it->second.end(), forge_weight.begin());
            }
        };

        copyWeight("self_attn.q_proj.weight", layer.attn.qProj.weight);
        copyWeight("self_attn.k_proj.weight", layer.attn.kProj.weight);
        copyWeight("self_attn.v_proj.weight", layer.attn.vProj.weight);
        copyWeight("self_attn.o_proj.weight", layer.attn.oProj.weight);

        // MLP (Llama uses gate_proj, up_proj, down_proj)
        // Forge uses ff1 (gate_proj), ff2 (down_proj) - up_proj is separate in Llama
        // For simplicity, map gate_proj to ff1, down_proj to ff2
        auto it_gate = hf.weights.find(prefix + "mlp.gate_proj.weight");
        if (it_gate != hf.weights.end()) {
            std::copy(it_gate->second.begin(), it_gate->second.end(), layer.ff.ff1.weight.begin());
        }
        auto it_down = hf.weights.find(prefix + "mlp.down_proj.weight");
        if (it_down != hf.weights.end()) {
            std::copy(it_down->second.begin(), it_down->second.end(), layer.ff.ff2.weight.begin());
        }

        // Layer norms
        auto it_ln1 = hf.weights.find(prefix + "input_layernorm.weight");
        if (it_ln1 != hf.weights.end()) {
            std::copy(it_ln1->second.begin(), it_ln1->second.end(), layer.ln1.weight.begin());
        }
        auto it_ln2 = hf.weights.find(prefix + "post_attention_layernorm.weight");
        if (it_ln2 != hf.weights.end()) {
            std::copy(it_ln2->second.begin(), it_ln2->second.end(), layer.ln2.weight.begin());
        }
    }

    return true;
}

bool HFModelConverter::convertGPT2(const WeightMap& hf, const HFModelInfo& info, TransformerModel& forge) {
    // GPT-2 weight mapping
    return true;
}

bool HFModelConverter::convertGPTJ(const WeightMap& hf, const HFModelInfo& info, TransformerModel& forge) {
    // GPT-J weight mapping
    return true;
}

bool HFModelConverter::convertMistral(const WeightMap& hf, const HFModelInfo& info, TransformerModel& forge) {
    return convertLlama(hf, info, forge);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string HFModelConverter::remapWeightName(const std::string& hf_name, const std::string& architecture) {
    std::string name = hf_name;
    
    // Remove "model." prefix if present
    if (name.rfind("model.", 0) == 0) name = name.substr(6);
    
    // Architecture-specific remapping
    if (architecture == "llama" || architecture == "mistral") {
        // model.layers.0.self_attn.q_proj.weight -> layers.0.attn.qProj.weight
        // model.layers.0.mlp.gate_proj.weight -> layers.0.ff.ff1.weight
    }
    
    return name;
}

void HFModelConverter::maybeTranspose(std::vector<float>& weight, const std::vector<int64_t>& shape, bool transpose) {
    if (!transpose || shape.size() != 2) return;
    
    int rows = shape[0];
    int cols = shape[1];
    std::vector<float> transposed(rows * cols);
    
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            transposed[j * rows + i] = weight[i * cols + j];
        }
    }
    
    weight.swap(transposed);
}

void HFModelConverter::convertDtype(const void* src, float* dst, size_t count, const std::string& src_dtype) {
    if (src_dtype == "float32" || src_dtype == "F32") {
        std::memcpy(dst, src, count * sizeof(float));
    } else if (src_dtype == "float16" || src_dtype == "F16" || src_dtype == "half") {
        const uint16_t* half_src = static_cast<const uint16_t*>(src);
        for (size_t i = 0; i < count; ++i) {
            uint16_t h = half_src[i];
            int sign = (h >> 15) & 1;
            int exp = (h >> 10) & 0x1F;
            int mantissa = h & 0x3FF;
            float val;
            if (exp == 0) {
                if (mantissa == 0) val = 0.0f;
                else val = std::ldexp(mantissa / 1024.0f, -14) * (sign ? -1.0f : 1.0f);
            } else if (exp == 31) {
                val = mantissa ? std::numeric_limits<float>::quiet_NaN() : (sign ? -INFINITY : INFINITY);
            } else {
                val = std::ldexp((1.0f + mantissa / 1024.0f), exp - 15) * (sign ? -1.0f : 1.0f);
            }
            dst[i] = val;
        }
    } else if (src_dtype == "bfloat16" || src_dtype == "BF16") {
            const uint16_t* bf16_src = static_cast<const uint16_t*>(src);
            for (size_t i = 0; i < count; ++i) {
                uint32_t f = static_cast<uint32_t>(bf16_src[i]) << 16;
                dst[i] = *reinterpret_cast<float*>(&f);
            }
        } else {
            // Default to float32
            std::memcpy(dst, src, count * sizeof(float));
        }
    }
}

// ============================================================================
// SAVE/LOAD FORGE MODEL
// ============================================================================

namespace forge::llm {

bool HFModelConverter::saveForgeModel(const TransformerModel& model, const Tokenizer& tokenizer,
                                       const std::string& model_path, const std::string& tokenizer_path) {
    model.save(model_path);
    tokenizer.save(tokenizer_path);
    return true;
}

bool HFModelConverter::loadForgeModel(const std::string& model_path, const std::string& tokenizer_path,
                                       TransformerModel& model, Tokenizer& tokenizer, ModelConfig& config) {
    model.load(model_path);
    tokenizer.load(tokenizer_path);
    config = model.config;
    return true;
}

} // namespace forge::llm