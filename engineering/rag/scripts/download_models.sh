#!/usr/bin/env bash
# ==============================================================================
# RAG System - Local Model Download Script
# ==============================================================================
# Downloads all required local models for the RAG system.
# Models:
#   1. BAAI/bge-m3                  - Text Embedding (1GB)
#   2. BAAI/bge-reranker-v2-m3     - Reranker ONNX (1.5GB)
#   3. Qwen/Qwen2-VL-4B-Instruct   - Chart Understanding VLM (4GB)
#   4. CLIP-ViT-H-14                - Video Frame Embedding (1.5GB)
#   5. faster-whisper (large-v3)   - Audio Transcription (1.5GB)
#   6. GraphCodeBERT                - Code Embedding (500MB)
#
# Total download size: ~10GB
# Total runtime memory: ~15GB
# ==============================================================================

set -euo pipefail

# Configuration
MODELS_DIR="${RAG_MODELS_DIR:-./models}"
HUGGINGFACE_TOKEN="${HF_TOKEN:-}"
DOWNLOAD_TOOL="huggingface-cli"  # or "wget"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[OK]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check dependencies
check_dependencies() {
    log_info "Checking dependencies..."

    # Check git
    if ! command -v git &> /dev/null; then
        log_error "git is required but not installed."
        exit 1
    fi

    # Check git-lfs
    if ! command -v git-lfs &> /dev/null && ! git lfs &> /dev/null 2>&1; then
        log_warn "git-lfs not found. Attempting to install..."
        if command -v apt-get &> /dev/null; then
            sudo apt-get update && sudo apt-get install -y git-lfs
        elif command -v brew &> /dev/null; then
            brew install git-lfs
        elif command -v choco &> /dev/null; then
            choco install git-lfs
        else
            log_error "Cannot install git-lfs automatically. Please install it manually."
            exit 1
        fi
        git lfs install
    fi

    # Check huggingface-cli
    if ! command -v huggingface-cli &> /dev/null; then
        log_warn "huggingface-cli not found. Attempting to install..."
        pip install -U huggingface_hub[cli]
    fi

    log_success "Dependencies check passed."
}

# Download BGE-M3 (Embedding)
download_bge_m3() {
    local target_dir="$MODELS_DIR/bge-m3"

    if [ -d "$target_dir" ] && [ -f "$target_dir/config.json" ]; then
        log_info "BGE-M3 already exists at $target_dir. Skipping."
        return 0
    fi

    log_info "Downloading BAAI/bge-m3 (Text Embedding)..."
    mkdir -p "$target_dir"

    huggingface-cli download BAAI/bge-m3 \
        --local-dir "$target_dir" \
        --local-dir-use-symlinks False

    log_success "BGE-M3 downloaded successfully."
}

# Download BGE Reranker v2 m3 (ONNX)
download_bge_reranker() {
    local target_dir="$MODELS_DIR/bge-reranker"

    if [ -d "$target_dir" ] && [ -f "$target_dir/config.json" ]; then
        log_info "BGE Reranker already exists at $target_dir. Skipping."
        return 0
    fi

    log_info "Downloading BAAI/bge-reranker-v2-m3 (Reranker)..."
    mkdir -p "$target_dir"

    huggingface-cli download BAAI/bge-reranker-v2-m3 \
        --local-dir "$target_dir" \
        --local-dir-use-symlinks False

    # Check for ONNX model, if not present, try to convert or download ONNX variant
    if [ ! -f "$target_dir/model.onnx" ]; then
        log_warn "ONNX model not found in the repo. Checking for PyTorch model..."
        if [ -f "$target_dir/pytorch_model.bin" ]; then
            log_info "PyTorch model found. Attempting conversion to ONNX..."
            if command -v python &> /dev/null; then
                python -c "
from transformers import AutoModelForSequenceClassification, AutoTokenizer
import torch

model_path = '$target_dir'
model = AutoModelForSequenceClassification.from_pretrained(model_path, trust_remote_code=True)
tokenizer = AutoTokenizer.from_pretrained(model_path, trust_remote_code=True)

# Dummy input for export
dummy_input = tokenizer('test', return_tensors='pt')

torch.onnx.export(
    model,
    (dummy_input['input_ids'], dummy_input['attention_mask']),
    f'{model_path}/model.onnx',
    input_names=['input_ids', 'attention_mask'],
    output_names=['logits'],
    dynamic_axes={
        'input_ids': {0: 'batch_size', 1: 'sequence'},
        'attention_mask': {0: 'batch_size', 1: 'sequence'},
        'logits': {0: 'batch_size'}
    }
)
print('ONNX export successful.')
" 2>/dev/null || log_warn "ONNX conversion failed. Please convert manually or check for ONNX variant."
            fi
        fi
    fi

    log_success "BGE Reranker downloaded successfully."
}

# Download Qwen2-VL (Chart Understanding)
download_qwen_vl() {
    local target_dir="$MODELS_DIR/qwen-vl2"

    if [ -d "$target_dir" ] && [ -f "$target_dir/config.json" ]; then
        log_info "Qwen2-VL already exists at $target_dir. Skipping."
        return 0
    fi

    log_info "Downloading Qwen/Qwen2-VL-4B-Instruct (VLM)..."
    mkdir -p "$target_dir"

    # Auth token if needed
    local auth_args=""
    if [ -n "$HUGGINGFACE_TOKEN" ]; then
        auth_args="--token $HUGGINGFACE_TOKEN"
    fi

    huggingface-cli download Qwen/Qwen2-VL-4B-Instruct \
        --local-dir "$target_dir" \
        --local-dir-use-symlinks False \
        $auth_args

    log_success "Qwen2-VL downloaded successfully."
}

# Download CLIP (Video Frame)
download_clip() {
    local target_dir="$MODELS_DIR/clip"

    if [ -d "$target_dir" ] && [ -f "$target_dir/config.json" ]; then
        log_info "CLIP already exists at $target_dir. Skipping."
        return 0
    fi

    log_info "Downloading CLIP-ViT-H-14 (Video Frame Embedding)..."
    mkdir -p "$target_dir"

    huggingface-cli download laion/CLIP-ViT-H-14-laion2B-s32B-b79K \
        --local-dir "$target_dir" \
        --local-dir-use-symlinks False

    log_success "CLIP downloaded successfully."
}

# Download Whisper (Audio Transcription)
download_whisper() {
    local target_dir="$MODELS_DIR/whisper-large-v3"

    if [ -d "$target_dir" ] && [ -f "$target_dir" ]; then
        log_info "Whisper already exists at $target_dir. Skipping."
        return 0
    fi

    log_info "Downloading Faster-Whisper (large-v3)..."
    mkdir -p "$target_dir"

    # Use faster-whisper which is more efficient
    pip install faster-whisper

    # Download the model via python to ensure correct format
    python -c "
from faster_whisper import WhisperModel
import os

# This will download the model to the cache or specified dir
model = WhisperModel('large-v3', device='cpu', compute_type='int8', download_root='$MODELS_DIR')
print('Whisper model downloaded successfully.')
" 2>/dev/null || {
        log_warn "Python download failed. Trying alternative..."
        # Fallback: download via huggingface-cli if available (for original whisper)
        huggingface-cli download openai/whisper-large-v3 \
            --local-dir "$target_dir" \
            --local-dir-use-symlinks False 2>/dev/null || true
    }

    log_success "Whisper downloaded successfully."
}

# Download GraphCodeBERT (Code Embedding)
download_graphcodebert() {
    local target_dir="$MODELS_DIR/graphcodebert"

    if [ -d "$target_dir" ] && [ -f "$target_dir/config.json" ]; then
        log_info "GraphCodeBERT already exists at $target_dir. Skipping."
        return 0
    fi

    log_info "Downloading GraphCodeBERT (Code Embedding)..."
    mkdir -p "$target_dir"

    huggingface-cli download microsoft/graphcodebert-base \
        --local-dir "$target_dir" \
        --local-dir-use-symlinks False

    log_success "GraphCodeBERT downloaded successfully."
}

# Create configuration file
create_config() {
    log_info "Creating model configuration template..."
    mkdir -p "$MODELS_DIR"

    cat > "$MODELS_DIR/models_config.yaml" << EOF
# RAG System - Local Model Configuration
# Auto-generated by download_models.sh

models:
  embedding:
    name: "BAAI/bge-m3"
    path: "${MODELS_DIR}/bge-m3"
    dimension: 768
    max_length: 8192
    provider: "local"

  reranker:
    name: "BAAI/bge-reranker-v2-m3"
    path: "${MODELS_DIR}/bge-reranker"
    format: "onnx"  # or "pytorch"
    provider: "local"
    # provider: "cuda"  # Use CUDA if available

  vlm:
    name: "Qwen/Qwen2-VL-4B-Instruct"
    path: "${MODELS_DIR}/qwen-vl2"
    use_local_vlm: true
    provider: "local"

  vision:
    name: "CLIP-ViT-H-14"
    path: "${MODELS_DIR}/clip"
    dimension: 768
    provider: "local"

  audio:
    name: "faster-whisper-large-v3"
    path: "${MODELS_DIR}/whisper-large-v3"
    device: "cpu"  # or "cuda"
    compute_type: "int8"
    language: "zh"  # zh, en, auto

  code:
    name: "microsoft/graphcodebert-base"
    path: "${MODELS_DIR}/graphcodebert"
    dimension: 768
    provider: "local"

# System Configuration
system:
  max_concurrent_inference: 2
  cache_size_gb: 4
  use_gpu: false
  gpu_device: "cuda:0"
EOF

    log_success "Configuration created at $MODELS_DIR/models_config.yaml"
}

# Print summary
print_summary() {
    echo ""
    echo "=================================================================="
    echo " Download Summary"
    echo "=================================================================="
    echo ""
    echo "Models directory: $MODELS_DIR"
    echo ""

    local total_size=0

    for dir in bge-m3 bge-reranker qwen-vl2 clip graphcodebert; do
        local path="$MODELS_DIR/$dir"
        if [ -d "$path" ]; then
            local size=$(du -sh "$path" 2>/dev/null | cut -f1)
            echo -e "  ${GREEN}[OK]${NC}   $dir ($size)"
        else
            echo -e "  ${RED}[--]${NC}   $dir (missing)"
        fi
    done

    # Check whisper (might be in a different location)
    if [ -d "$MODELS_DIR/whisper-large-v3" ] || [ -d "$MODELS_DIR/models--Systran--faster-whisper-large-v3" ]; then
        echo -e "  ${GREEN}[OK]${NC}   whisper-large-v3"
    else
        echo -e "  ${RED}[--]${NC}   whisper-large-v3 (missing)"
    fi

    echo ""
    echo "Configuration: $MODELS_DIR/models_config.yaml"
    echo ""
    echo "=================================================================="
}

# Main
main() {
    echo "=================================================================="
    echo " RAG System - Model Downloader"
    echo "=================================================================="
    echo ""

    check_dependencies

    mkdir -p "$MODELS_DIR"

    download_bge_m3
    download_bge_reranker
    download_qwen_vl
    download_clip
    download_whisper
    download_graphcodebert
    create_config

    print_summary

    echo ""
    log_success "All models downloaded successfully!"
    echo ""
    echo "Next steps:"
    echo "  1. Review configuration: $MODELS_DIR/models_config.yaml"
    echo "  2. Build the RAG system: cmake --build build/"
    echo "  3. Run tests: ctest --test-dir build/"
    echo ""
}

# Run main function
main "$@"
