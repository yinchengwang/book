#!/bin/bash
# kbase 模型下载脚本
# 用法: bash reference/models/download_model.sh [model_name]
#   model_name: minilm-l6 (默认), bge-small-zh

set -e

MODEL_NAME="${1:-minilm-l6}"
TARGET_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "下载模型: $MODEL_NAME"
echo "目标目录: $TARGET_DIR"

case "$MODEL_NAME" in
    minilm-l6)
        # MiniLM-L6 GGUF（384 维 Embedding）
        URL="https://huggingface.co/second-state/All-MiniLM-L6-v2-Embedding-GGUF/resolve/main/all-MiniLM-L6-v2-q4_k_m.gguf"
        OUTPUT="$TARGET_DIR/minilm-l6-q4_k_m.gguf"
        ;;
    bge-small-zh)
        # BGE-small-zh GGUF（中文 Embedding，512 维）
        URL="https://huggingface.co/CompendiumLabs/bge-small-en-v1.5-gguf/resolve/main/bge-small-en-v1.5-q4_k_m.gguf"
        OUTPUT="$TARGET_DIR/bge-small-en-q4_k_m.gguf"
        ;;
    *)
        echo "未知模型: $MODEL_NAME"
        echo "支持: minilm-l6, bge-small-zh"
        exit 1
        ;;
esac

if [ -f "$OUTPUT" ]; then
    echo "模型已存在: $OUTPUT"
    exit 0
fi

echo "下载: $URL"
wget -O "$OUTPUT" "$URL"

echo "完成: $OUTPUT"
ls -lh "$OUTPUT"
