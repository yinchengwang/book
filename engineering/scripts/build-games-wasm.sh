#!/bin/bash
#
# build-games-wasm.sh - 将 games_core 编译为 WebAssembly
#
# 用法: bash engineering/scripts/build-games-wasm.sh
#
# 前置条件: emsdk 已安装并激活 (https://emscripten.org/docs/getting_started/downloads.html)
#

set -e

# 工程根目录: scripts/ -> engineering/ -> 项目根
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 检查 Emscripten 编译器是否可用
if ! command -v emcc &> /dev/null; then
    echo "错误: Emscripten SDK 未安装或未激活" >&2
    echo "安装方法: https://emscripten.org/docs/getting_started/downloads.html" >&2
    exit 1
fi

OUTPUT_DIR="$PROJECT_ROOT/engineering/apps/games/web"
mkdir -p "$OUTPUT_DIR"

echo "编译 games_core 为 WASM..."

emcc -o "$OUTPUT_DIR/games.js" \
    "$PROJECT_ROOT/engineering/apps/games/wasm/wasm_binding.c" \
    "$PROJECT_ROOT/engineering/apps/games/core/g2048_core.c" \
    "$PROJECT_ROOT/engineering/apps/games/core/snake_core.c" \
    -lm \
    -s EXPORTED_FUNCTIONS="['_g2048_create','_g2048_move','_g2048_tile','_g2048_score','_g2048_game_over','_g2048_won','_g2048_can_move','_snake_create','_snake_tick','_snake_input_dir','_snake_body_count','_snake_body_x_at','_snake_body_y_at','_snake_food_x','_snake_food_y','_snake_score_val','_snake_over']" \
    -s MODULARIZE=1 \
    -s EXPORT_NAME="GameModule" \
    -s ALLOW_MEMORY_GROWTH=1 \
    -O2

echo "WASM 编译完成: $OUTPUT_DIR/games.js, $OUTPUT_DIR/games.wasm"
