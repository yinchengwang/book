/**
 * @file minilm_embedder.h
 * @brief MiniLM-L6 embedding inference (pure C++, no external libs)
 *
 * Loads FP32 weights converted from GGUF by scripts/gguf_to_fp32.py,
 * performs WordPiece tokenization + MiniLM-L6 Transformer forward pass,
 * and returns 384-dim L2-normalized embeddings.
 *
 * Architecture:
 *   - Embeddings: token + position + type
 *   - 6x Transformer encoder layers (12 heads, 384 dim)
 *   - Mean pooling over token embeddings
 *   - L2 normalization
 *
 * Reference: sentence-transformers/all-MiniLM-L6-v2
 */

#pragma once

#include <string>
#include <vector>
#include <memory>

namespace mmrag {

/**
 * @brief Pure C++ MiniLM-L6 embedder.
 *
 * Usage:
 *   auto embedder = std::make_unique<MiniLMEmbedder>(
 *       "path/to/fp32_out",  // directory containing weights.bin + vocab.txt + meta.json
 *       384                  // embedding dim
 *   );
 *   if (embedder->is_ready()) {
 *       auto vec = embedder->encode("hello world");
 *       // vec.size() == 384, L2-normalized
 *   }
 */
class MiniLMEmbedder {
public:
    /**
     * @brief Construct from FP32 directory produced by gguf_to_fp32.py.
     * @param fp32_dir Path to directory with weights.bin, vocab.txt, meta.json
     * @param dimension Expected embedding dimension (must match meta.json)
     */
    MiniLMEmbedder(const std::string& fp32_dir, int dimension);

    ~MiniLMEmbedder();

    MiniLMEmbedder(const MiniLMEmbedder&) = delete;
    MiniLMEmbedder& operator=(const MiniLMEmbedder&) = delete;

    /**
     * @brief Check if model loaded successfully.
     */
    bool is_ready() const { return ready_; }

    /**
     * @brief Get embedding dimension.
     */
    int dimension() const { return dimension_; }

    /**
     * @brief Get model name (from meta.json).
     */
    const std::string& model_name() const { return model_name_; }

    /**
     * @brief Encode single text → 384-dim L2-normalized vector.
     * @param text Input text (will be lowercased + WordPiece tokenized)
     * @return Embedding vector; empty if not ready or encode fails
     */
    std::vector<float> encode(const std::string& text);

    /**
     * @brief Batch encode.
     * @param texts List of input texts
     * @return List of embedding vectors, order matches input
     */
    std::vector<std::vector<float>> encode_batch(const std::vector<std::string>& texts);

    // WordPiece tokenization: lowercase → split → greedy match against vocab
    // (public for debugging / comparison with Python tokenizer)
    std::vector<int> tokenize(const std::string& text);

    // Run MiniLM forward pass on input_ids (already tokenized)
    // (public for debugging / feeding known token IDs)
    std::vector<float> forward(const std::vector<int>& input_ids);

private:
    // Load metadata + weights + vocab from disk
    bool load();

    // Helper: matrix-vector multiply y = W @ x (row-major, W is [rows, cols])
    void matmul(const float* W, const float* x, float* y, int rows, int cols);

    // Helper: add bias to vector in-place
    void add_bias(float* y, const float* b, int n);

    // LayerNorm: x = (x - mean) / sqrt(var + eps) * gamma + beta
    void layer_norm(const float* x, const float* gamma, const float* beta,
                    float* y, int n, float eps);

    // GELU activation (tanh approximation)
    void gelu(float* x, int n);

    // Slice rows from a [rows, cols] matrix
    // row i, j = src[i*cols + j]
    const float* row(const float* mat, int row, int cols) {
        return mat + (size_t)row * cols;
    }

    // -------- Configuration --------
    std::string fp32_dir_;
    int dimension_ = 384;

    // -------- Loaded model --------
    bool ready_ = false;
    std::string model_name_;
    int n_layer_ = 6;
    int n_head_ = 12;
    int n_embd_ = 384;
    int n_ff_ = 1536;
    int n_ctx_ = 512;
    int n_vocab_ = 0;
    float layer_norm_eps_ = 1e-12f;
    bool use_tanh_gelu_ = true;  // true for BERT-style; false for LLaMA-style

    // mmap'd weights file (POSIX) or heap buffer (Windows)
    int weights_fd_ = -1;
    size_t weights_size_ = 0;
    const float* weights_ = nullptr;
    std::vector<float> weights_buf_;  // Windows backing storage

    // vocab: each token is a UTF-8 string
    std::vector<std::string> vocab_;

    // Offsets into weights.bin (from meta.json)
    int64_t off_token_embd_ = 0;
    int64_t off_position_embd_ = 0;
    int64_t off_token_types_ = 0;
    int64_t off_token_embd_norm_w_ = 0;
    int64_t off_token_embd_norm_b_ = 0;

    // Per-block offsets (arrays of size n_layer_)
    std::vector<int64_t> off_attn_q_w_;
    std::vector<int64_t> off_attn_q_b_;
    std::vector<int64_t> off_attn_k_w_;
    std::vector<int64_t> off_attn_k_b_;
    std::vector<int64_t> off_attn_v_w_;
    std::vector<int64_t> off_attn_v_b_;
    std::vector<int64_t> off_attn_out_w_;
    std::vector<int64_t> off_attn_out_b_;
    std::vector<int64_t> off_attn_norm_w_;
    std::vector<int64_t> off_attn_norm_b_;
    std::vector<int64_t> off_ffn_up_w_;
    std::vector<int64_t> off_ffn_up_b_;
    std::vector<int64_t> off_ffn_down_w_;
    std::vector<int64_t> off_ffn_down_b_;
    std::vector<int64_t> off_layer_out_norm_w_;
    std::vector<int64_t> off_layer_out_norm_b_;
};

}  // namespace mmrag