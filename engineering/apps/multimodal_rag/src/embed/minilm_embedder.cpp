/**
 * @file minilm_embedder.cpp
 * @brief MiniLM-L6 forward pass implementation
 */

#include "mmrag/minilm_embedder.h"
#include "mmrag/logger.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <map>
#ifndef _WIN32
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace mmrag {

namespace {

// Convert a UTF-8 string to lowercase ASCII (best-effort for ASCII input)
std::string to_lower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (c >= 'A' && c <= 'Z') {
            out.push_back(c + 32);
        } else {
            out.push_back(c);
        }
    }
    return out;
}

// Split on whitespace and punctuation (BERT-style basic tokenizer)
std::vector<std::string> basic_tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string cur;
    auto flush = [&]() {
        if (!cur.empty()) {
            tokens.push_back(cur);
            cur.clear();
        }
    };
    for (size_t i = 0; i < text.size(); ) {
        unsigned char c = text[i];
        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            flush();
            i++;
            continue;
        }
        // Multi-byte UTF-8 handling: pass through non-ASCII bytes
        if (c < 0x80) {
            // ASCII punctuation
            if (ispunct(c)) {
                flush();
                tokens.push_back(std::string(1, c));
                i++;
            } else {
                cur.push_back(c);
                i++;
            }
        } else {
            // UTF-8 multi-byte: read whole codepoint
            int len = 1;
            if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) len = 4;
            for (int k = 0; k < len && i < text.size(); k++) {
                cur.push_back(text[i++]);
            }
        }
    }
    flush();
    return tokens;
}

// Soft max for attention scores
void softmax(float* x, int n) {
    if (n <= 0) return;
    float max_val = x[0];
    for (int i = 1; i < n; i++) if (x[i] > max_val) max_val = x[i];
    float sum = 0;
    for (int i = 0; i < n; i++) {
        x[i] = std::exp(x[i] - max_val);
        sum += x[i];
    }
    /* Guard against division by zero (all-zero input case) */
    if (sum > 1e-30f) {
        float inv_sum = 1.0f / sum;
        for (int i = 0; i < n; i++) x[i] *= inv_sum;
    } else {
        /* Uniform distribution when all exp() terms underflow to zero */
        float uniform = 1.0f / n;
        for (int i = 0; i < n; i++) x[i] = uniform;
    }
}

}  // namespace

MiniLMEmbedder::MiniLMEmbedder(const std::string& fp32_dir, int dimension)
    : fp32_dir_(fp32_dir), dimension_(dimension) {
    ready_ = load();
    if (ready_) {
        RAG_INFO("MiniLMEmbedder loaded: dim=" + std::to_string(dimension_) +
                 ", n_layer=" + std::to_string(n_layer_) +
                 ", vocab=" + std::to_string(n_vocab_));
    } else {
        RAG_WARN("MiniLMEmbedder failed to load from " + fp32_dir);
    }
}

MiniLMEmbedder::~MiniLMEmbedder() {
#ifdef _WIN32
    weights_buf_.clear();
    weights_ = nullptr;
#else
    if (weights_) {
        munmap(const_cast<float*>(weights_), weights_size_);
    }
    if (weights_fd_ >= 0) {
        close(weights_fd_);
    }
#endif
}

bool MiniLMEmbedder::load() {
    namespace fs = std::filesystem;

    fs::path dir(fp32_dir_);
    fs::path vocab_path = dir / "vocab.txt";
    fs::path weights_path = dir / "weights.bin";
    fs::path meta_path = dir / "meta.json";

    if (!fs::exists(vocab_path) || !fs::exists(weights_path) || !fs::exists(meta_path)) {
        RAG_WARN("MiniLMEmbedder: missing files in " + fp32_dir_);
        return false;
    }

    // Load meta.json
    {
        std::ifstream f(meta_path);
        if (!f.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());

        // Simple key extraction (avoid nlohmann/json dep)
        auto find_str = [&](const std::string& key) -> std::string {
            size_t pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return "";
            pos = content.find(":", pos);
            if (pos == std::string::npos) return "";
            pos = content.find("\"", pos);
            if (pos == std::string::npos) return "";
            size_t end = content.find("\"", pos + 1);
            return content.substr(pos + 1, end - pos - 1);
        };
        auto find_int = [&](const std::string& key) -> int {
            size_t pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return 0;
            pos = content.find(":", pos);
            if (pos == std::string::npos) return 0;
            pos++;
            while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\n' || content[pos] == '\t')) pos++;
            int val = 0;
            while (pos < content.size() && content[pos] >= '0' && content[pos] <= '9') {
                val = val * 10 + (content[pos] - '0');
                pos++;
            }
            return val;
        };
        auto find_off = [&](const std::string& name) -> int64_t {
            size_t pos = content.find("\"" + name + "\"");
            if (pos == std::string::npos) return -1;
            pos = content.find("\"offset\"", pos);
            if (pos == std::string::npos) return -1;
            pos = content.find(":", pos);
            if (pos == std::string::npos) return -1;
            pos++;
            while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\n' || content[pos] == '\t')) pos++;
            int64_t val = 0;
            while (pos < content.size() && content[pos] >= '0' && content[pos] <= '9') {
                val = val * 10 + (content[pos] - '0');
                pos++;
            }
            return val;
        };

        model_name_ = find_str("general.name");
        if (model_name_.empty()) model_name_ = find_str("name");
        if (model_name_.empty()) model_name_ = "minilm";

        // meta.json fields are flat (gguf_to_fp32.py flattens model.*)
        n_layer_ = find_int("block_count");
        n_head_  = find_int("attention.head_count");
        n_embd_  = find_int("embedding_length");
        n_ff_    = find_int("feed_forward_length");
        n_ctx_   = find_int("context_length");
        n_vocab_ = find_int("vocab_size");

        // Check GGUF metadata for GELU type (BERT uses tanh approx; LLaMA-style uses standard GELU)
        std::string ffn_type = find_str("bert.ffn_type");
        if (ffn_type.empty()) ffn_type = find_str("general.architecture");
        use_tanh_gelu_ = (ffn_type.find("bert") != std::string::npos ||
                          ffn_type.find("nomic-bert") != std::string::npos ||
                          ffn_type.find("jina-bert") != std::string::npos);

        // Guard against uninitialized hyperparameters (parsing failures)
        if (n_head_ <= 0) {
            // Auto-detect: try 16, 12, 8 heads based on n_embd
            if (n_embd_ == 1024) n_head_ = 16;      // BGE-large
            else if (n_embd_ == 768) n_head_ = 12;  // BGE-base
            else if (n_embd_ == 384) n_head_ = 12;  // MiniLM
            else n_head_ = 8;
        }
        if (n_layer_ <= 0) n_layer_ = 6;
        if (n_embd_ <= 0) n_embd_ = 384;
        if (n_ff_ <= 0) n_ff_ = 1536;
        if (n_ctx_ <= 0) n_ctx_ = 512;

        if (n_embd_ != dimension_) {
            RAG_WARN("MiniLMEmbedder: dimension mismatch (config=" +
                     std::to_string(dimension_) + " model=" +
                     std::to_string(n_embd_) + ")");
            return false;
        }

        off_token_embd_       = find_off("token_embd.weight");
        off_position_embd_    = find_off("position_embd.weight");
        off_token_types_      = find_off("token_types.weight");
        off_token_embd_norm_w_ = find_off("token_embd_norm.weight");
        off_token_embd_norm_b_ = find_off("token_embd_norm.bias");

        // Allocate per-block offset arrays
        off_attn_q_w_.resize(n_layer_);
        off_attn_q_b_.resize(n_layer_);
        off_attn_k_w_.resize(n_layer_);
        off_attn_k_b_.resize(n_layer_);
        off_attn_v_w_.resize(n_layer_);
        off_attn_v_b_.resize(n_layer_);
        off_attn_out_w_.resize(n_layer_);
        off_attn_out_b_.resize(n_layer_);
        off_attn_norm_w_.resize(n_layer_);
        off_attn_norm_b_.resize(n_layer_);
        off_ffn_up_w_.resize(n_layer_);
        off_ffn_up_b_.resize(n_layer_);
        off_ffn_down_w_.resize(n_layer_);
        off_ffn_down_b_.resize(n_layer_);
        off_layer_out_norm_w_.resize(n_layer_);
        off_layer_out_norm_b_.resize(n_layer_);

        for (int l = 0; l < n_layer_; l++) {
            std::string p = "blk." + std::to_string(l) + ".";
            off_attn_q_w_[l]         = find_off(p + "attn_q.weight");
            off_attn_q_b_[l]         = find_off(p + "attn_q.bias");
            off_attn_k_w_[l]         = find_off(p + "attn_k.weight");
            off_attn_k_b_[l]         = find_off(p + "attn_k.bias");
            off_attn_v_w_[l]         = find_off(p + "attn_v.weight");
            off_attn_v_b_[l]         = find_off(p + "attn_v.bias");
            off_attn_out_w_[l]      = find_off(p + "attn_output.weight");
            off_attn_out_b_[l]      = find_off(p + "attn_output.bias");
            off_attn_norm_w_[l]     = find_off(p + "attn_norm.weight");
            off_attn_norm_b_[l]     = find_off(p + "attn_norm.bias");
            off_ffn_up_w_[l]         = find_off(p + "ffn_up.weight");
            off_ffn_up_b_[l]         = find_off(p + "ffn_up.bias");
            off_ffn_down_w_[l]       = find_off(p + "ffn_down.weight");
            off_ffn_down_b_[l]       = find_off(p + "ffn_down.bias");
            off_layer_out_norm_w_[l] = find_off(p + "layer_output_norm.weight");
            off_layer_out_norm_b_[l] = find_off(p + "layer_output_norm.bias");
        }
    }

    // Load vocab.txt
    {
        std::ifstream f(vocab_path);
        if (!f.is_open()) return false;
        std::string line;
        while (std::getline(f, line)) {
            // Strip trailing \r
            if (!line.empty() && line.back() == '\r') line.pop_back();
            vocab_.push_back(line);
        }
        if ((int)vocab_.size() != n_vocab_) {
            RAG_WARN("MiniLMEmbedder: vocab mismatch: file=" +
                     std::to_string(vocab_.size()) + " meta=" +
                     std::to_string(n_vocab_));
            // Continue but warn
        }
    }

    // Build vocab lookup: token -> id
    std::map<std::string, int> vocab_map;
    for (int i = 0; i < (int)vocab_.size(); i++) {
        vocab_map[vocab_[i]] = i;
    }

    // Read weights.bin (mmap on POSIX, std::ifstream on Windows)
    {
#ifdef _WIN32
        // Windows: simple std::ifstream read (86MB is fast enough)
        std::ifstream wf(weights_path.string().c_str(), std::ios::binary | std::ios::ate);
        if (!wf.is_open()) {
            RAG_WARN("MiniLMEmbedder: cannot open weights.bin");
            return false;
        }
        weights_size_ = wf.tellg();
        wf.seekg(0);
        weights_buf_.resize(weights_size_ / sizeof(float) + 1);  // +1 to avoid zero-size
        wf.read(reinterpret_cast<char*>(weights_buf_.data()), weights_size_);
        weights_ = weights_buf_.data();
        wf.close();
#else
        weights_fd_ = open(weights_path.c_str(), O_RDONLY);
        if (weights_fd_ < 0) {
            RAG_WARN("MiniLMEmbedder: cannot open weights.bin");
            return false;
        }
        struct stat st;
        if (fstat(weights_fd_, &st) < 0) {
            close(weights_fd_);
            weights_fd_ = -1;
            return false;
        }
        weights_size_ = st.st_size;
        weights_ = (const float*)mmap(nullptr, weights_size_, PROT_READ, MAP_PRIVATE, weights_fd_, 0);
        if (weights_ == MAP_FAILED) {
            weights_ = nullptr;
            close(weights_fd_);
            weights_fd_ = -1;
            return false;
        }
#endif
    }

    return true;
}

std::vector<int> MiniLMEmbedder::tokenize(const std::string& text) {
    // BERT WordPiece tokenization
    // 1. lowercase + basic tokenize
    // 2. for each token, greedy longest-match WordPiece against vocab
    // 3. prepend [CLS], append [SEP]

    // Build vocab lookup lazily here (could cache in load())
    // For simplicity, just search linearly (vocab is ~30K, tokenize is not hot path)
    auto vocab_lookup = [&](const std::string& s) -> int {
        for (int i = 0; i < (int)vocab_.size(); i++) {
            if (vocab_[i] == s) return i;
        }
        return -1;
    };

    std::string lower = to_lower(text);
    std::vector<std::string> words = basic_tokenize(lower);

    std::vector<int> ids;
    ids.push_back(101);  // [CLS]

    int unk_id = vocab_lookup("[unk]");  // [UNK] = 100 in BERT vocab
    for (const auto& word : words) {
        if (word.empty()) continue;
        // Truncate very long tokens
        if ((int)word.size() > 100) {
            if (unk_id >= 0) ids.push_back(unk_id);
            continue;
        }
        int start = 0;
        std::vector<int> word_ids;
        bool is_bad = false;
        while (start < (int)word.size()) {
            int end = word.size();
            int cur_id = -1;
            while (start < end) {
                std::string sub = word.substr(start, end - start);
                if (start > 0) sub = "##" + sub;
                int id = vocab_lookup(sub);
                if (id >= 0) {
                    cur_id = id;
                    break;
                }
                end--;
            }
            if (cur_id < 0) {
                is_bad = true;
                break;
            }
            word_ids.push_back(cur_id);
            start = end;
        }
        if (is_bad) {
            if (unk_id >= 0) ids.push_back(unk_id);
        } else {
            ids.insert(ids.end(), word_ids.begin(), word_ids.end());
        }
    }

    ids.push_back(102);  // [SEP]

    // Truncate to max position embeddings (n_ctx)
    if ((int)ids.size() > n_ctx_) {
        ids.resize(n_ctx_);
        ids.back() = 102;  // ensure last is [SEP]
    }

    return ids;
}

void MiniLMEmbedder::matmul(const float* W, const float* x, float* y, int rows, int cols) {
    // y[i] = sum_j W[i, j] * x[j]
    for (int i = 0; i < rows; i++) {
        float sum = 0;
        const float* Wrow = W + (size_t)i * cols;
        for (int j = 0; j < cols; j++) {
            sum += Wrow[j] * x[j];
        }
        y[i] = sum;
    }
}

void MiniLMEmbedder::add_bias(float* y, const float* b, int n) {
    for (int i = 0; i < n; i++) y[i] += b[i];
}

void MiniLMEmbedder::layer_norm(const float* x, const float* gamma, const float* beta,
                               float* y, int n, float eps) {
    float mean = 0;
    for (int i = 0; i < n; i++) mean += x[i];
    mean /= n;
    float var = 0;
    for (int i = 0; i < n; i++) {
        float d = x[i] - mean;
        var += d * d;
    }
    var /= n;
    /* Guard against negative variance due to FP precision */
    if (var < 0) var = 0;
    float inv_std = 1.0f / std::sqrt(var + eps);
    for (int i = 0; i < n; i++) {
        y[i] = (x[i] - mean) * inv_std * gamma[i] + beta[i];
    }
}

void MiniLMEmbedder::gelu(float* x, int n) {
    // Tanh approximation (BERT, BGE) vs exact (LLaMA-style)
    for (int i = 0; i < n; i++) {
        float v = x[i];
        float clamped = std::max(-100.0f, std::min(100.0f, v));
        float g;
        if (use_tanh_gelu_) {
            // Tanh approximation: 0.5 * v * (1 + tanh(sqrt(2/pi) * (v + 0.044715 * v^3)))
            g = 0.5f * v * (1.0f + std::tanh(
                0.7978845608f * (clamped + 0.044715f * clamped * clamped * clamped)
            ));
        } else {
            // Exact GELU (erf-based): 0.5 * v * (1 + erf(v / sqrt(2)))
            g = 0.5f * v * (1.0f + std::erf(clamped * 0.7071067811865475f));
        }
        if (g != g) g = 0.0f;
        x[i] = g;
    }
}

std::vector<float> MiniLMEmbedder::forward(const std::vector<int>& input_ids) {
    int seq_len = (int)input_ids.size();
    if (seq_len == 0) return {};
    if (seq_len > n_ctx_) seq_len = n_ctx_;

    // Buffers
    std::vector<float> hidden(seq_len * n_embd_);  // [seq, embd]
    std::vector<float> qkv(3 * seq_len * n_embd_);
    std::vector<float> attn_out(seq_len * n_embd_);
    std::vector<float> ffn_mid(seq_len * n_ff_);

    // -------- 1. Embedding lookup --------
    // hidden[i, j] = token_embd[input_ids[i], j] + position_embd[i, j] + token_types[0, j]
    for (int i = 0; i < seq_len; i++) {
        int tok_id = input_ids[i];
        const float* tok_emb = weights_ + off_token_embd_ / 4 + (size_t)tok_id * n_embd_;
        const float* pos_emb = weights_ + off_position_embd_ / 4 + (size_t)i * n_embd_;
        const float* tt_emb = weights_ + off_token_types_ / 4;  // type 0
        for (int j = 0; j < n_embd_; j++) {
            hidden[i * n_embd_ + j] = tok_emb[j] + pos_emb[j] + tt_emb[j];
        }
    }

    // Embedding LayerNorm (in-place)
    for (int i = 0; i < seq_len; i++) {
        std::vector<float> normed(n_embd_);
        layer_norm(hidden.data() + (size_t)i * n_embd_,
                   weights_ + off_token_embd_norm_w_ / 4,
                   weights_ + off_token_embd_norm_b_ / 4,
                   normed.data(),
                   n_embd_, layer_norm_eps_);
        for (int j = 0; j < n_embd_; j++) {
            hidden[i * n_embd_ + j] = normed[j];
        }
    }

    // -------- 2. 6 Transformer encoder layers --------
    int head_dim = n_embd_ / n_head_;

    for (int l = 0; l < n_layer_; l++) {
        // -------- 2a. Self-attention (Pre-LayerNorm) --------
        // 1. Pre-norm: hidden → attn_input via LayerNorm
        std::vector<float> attn_input(seq_len * n_embd_);
        for (int i = 0; i < seq_len; i++) {
            layer_norm(hidden.data() + (size_t)i * n_embd_,
                       weights_ + off_attn_norm_w_[l] / 4,
                       weights_ + off_attn_norm_b_[l] / 4,
                       attn_input.data() + (size_t)i * n_embd_,
                       n_embd_, layer_norm_eps_);
        }

        // 2. Compute Q, K, V from attn_input
        std::vector<float> q(seq_len * n_embd_);
        std::vector<float> k(seq_len * n_embd_);
        std::vector<float> v(seq_len * n_embd_);

        // Compute Q for all positions (from attn_input, not hidden)
        for (int i = 0; i < seq_len; i++) {
            matmul(weights_ + off_attn_q_w_[l] / 4,
                   attn_input.data() + (size_t)i * n_embd_,
                   q.data() + (size_t)i * n_embd_,
                   n_embd_, n_embd_);
            add_bias(q.data() + (size_t)i * n_embd_,
                     weights_ + off_attn_q_b_[l] / 4, n_embd_);
        }
        // Compute K from attn_input
        for (int i = 0; i < seq_len; i++) {
            matmul(weights_ + off_attn_k_w_[l] / 4,
                   attn_input.data() + (size_t)i * n_embd_,
                   k.data() + (size_t)i * n_embd_,
                   n_embd_, n_embd_);
            add_bias(k.data() + (size_t)i * n_embd_,
                     weights_ + off_attn_k_b_[l] / 4, n_embd_);
        }
        // Compute V from attn_input
        for (int i = 0; i < seq_len; i++) {
            matmul(weights_ + off_attn_v_w_[l] / 4,
                   attn_input.data() + (size_t)i * n_embd_,
                   v.data() + (size_t)i * n_embd_,
                   n_embd_, n_embd_);
            add_bias(v.data() + (size_t)i * n_embd_,
                     weights_ + off_attn_v_b_[l] / 4, n_embd_);
        }

        // Attention: for each head, Q @ K^T / sqrt(d_k), softmax, @ V
        std::vector<float> attn_out_seq(seq_len * n_embd_);
        float scale = 1.0f / std::sqrt((float)head_dim);

        for (int h = 0; h < n_head_; h++) {
            // Extract Q, K, V for this head: each is [seq, head_dim]
            // Score matrix: [seq_q, seq_k]
            std::vector<float> scores(seq_len * seq_len);
            for (int i = 0; i < seq_len; i++) {
                for (int j = 0; j < seq_len; j++) {
                    float s = 0;
                    for (int kk = 0; kk < head_dim; kk++) {
                        s += q[i * n_embd_ + h * head_dim + kk] *
                             k[j * n_embd_ + h * head_dim + kk];
                    }
                    scores[i * seq_len + j] = s * scale;
                }
            }
            // Softmax per row
            for (int i = 0; i < seq_len; i++) {
                softmax(scores.data() + (size_t)i * seq_len, seq_len);
            }
            // Multiply scores @ V for this head → [seq, head_dim]
            std::vector<float> head_out(seq_len * head_dim);
            for (int i = 0; i < seq_len; i++) {
                for (int j = 0; j < seq_len; j++) {
                    float s = scores[i * seq_len + j];
                    if (s == 0) continue;
                    for (int kk = 0; kk < head_dim; kk++) {
                        head_out[i * head_dim + kk] +=
                            s * v[j * n_embd_ + h * head_dim + kk];
                    }
                }
            }
            // Scatter into attn_out_seq
            for (int i = 0; i < seq_len; i++) {
                for (int kk = 0; kk < head_dim; kk++) {
                    attn_out_seq[i * n_embd_ + h * head_dim + kk] = head_out[i * head_dim + kk];
                }
            }
        }

        // Output projection: attn_out_seq @ attn_output.weight + bias
        std::vector<float> attn_proj(seq_len * n_embd_);
        for (int i = 0; i < seq_len; i++) {
            matmul(weights_ + off_attn_out_w_[l] / 4,
                   attn_out_seq.data() + (size_t)i * n_embd_,
                   attn_proj.data() + (size_t)i * n_embd_,
                   n_embd_, n_embd_);
            add_bias(attn_proj.data() + (size_t)i * n_embd_,
                     weights_ + off_attn_out_b_[l] / 4, n_embd_);
        }

        // Residual (no LayerNorm on output — Pre-LN only normalizes before sub-layers)
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < n_embd_; j++) {
                hidden[i * n_embd_ + j] += attn_proj[i * n_embd_ + j];
            }
        }

        // -------- 2b. FFN (Pre-LayerNorm) --------
        // Pre-norm: hidden → ffn_input via LayerNorm
        std::vector<float> ffn_input(seq_len * n_embd_);
        for (int i = 0; i < seq_len; i++) {
            layer_norm(hidden.data() + (size_t)i * n_embd_,
                       weights_ + off_layer_out_norm_w_[l] / 4,
                       weights_ + off_layer_out_norm_b_[l] / 4,
                       ffn_input.data() + (size_t)i * n_embd_,
                       n_embd_, layer_norm_eps_);
        }

        // FFN(x) = GELU(x @ W_up + b_up) @ W_down + b_down
        {
            std::vector<float> ffn_up(seq_len * n_ff_);
            for (int i = 0; i < seq_len; i++) {
                matmul(weights_ + off_ffn_up_w_[l] / 4,
                       ffn_input.data() + (size_t)i * n_embd_,
                       ffn_up.data() + (size_t)i * n_ff_,
                       n_ff_, n_embd_);
                add_bias(ffn_up.data() + (size_t)i * n_ff_,
                         weights_ + off_ffn_up_b_[l] / 4, n_ff_);
            }
            gelu(ffn_up.data(), (int)ffn_up.size());

            std::vector<float> ffn_down(seq_len * n_embd_);
            for (int i = 0; i < seq_len; i++) {
                matmul(weights_ + off_ffn_down_w_[l] / 4,
                       ffn_up.data() + (size_t)i * n_ff_,
                       ffn_down.data() + (size_t)i * n_embd_,
                       n_embd_, n_ff_);
                add_bias(ffn_down.data() + (size_t)i * n_embd_,
                         weights_ + off_ffn_down_b_[l] / 4, n_embd_);
            }

            // Residual (FFN output)
            for (int i = 0; i < seq_len; i++) {
                for (int j = 0; j < n_embd_; j++) {
                    hidden[i * n_embd_ + j] += ffn_down[i * n_embd_ + j];
                }
            }
        }
    }

    // -------- 3. Mean pooling --------
    // Average hidden states over all tokens
    std::vector<float> pooled(n_embd_, 0.0f);
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < n_embd_; j++) {
            pooled[j] += hidden[i * n_embd_ + j];
        }
    }
    for (int j = 0; j < n_embd_; j++) {
        pooled[j] /= seq_len;
    }

    // -------- 4. L2 normalize --------
    float norm = 0;
    for (int j = 0; j < n_embd_; j++) {
        norm += pooled[j] * pooled[j];
    }
    norm = std::sqrt(norm);
    if (norm > 1e-10f) {
        for (int j = 0; j < n_embd_; j++) {
            pooled[j] /= norm;
        }
    }

    return pooled;
}

std::vector<float> MiniLMEmbedder::encode(const std::string& text) {
    if (!ready_) return {};
    std::vector<int> ids = tokenize(text);
    return forward(ids);
}

std::vector<std::vector<float>> MiniLMEmbedder::encode_batch(
    const std::vector<std::string>& texts) {
    std::vector<std::vector<float>> results;
    results.reserve(texts.size());
    for (const auto& t : texts) {
        results.push_back(encode(t));
    }
    return results;
}

}  // namespace mmrag