#!/usr/bin/env bash
set -euo pipefail

EMCC="D:/code/book/emsdk/upstream/emscripten/emcc.exe"
WASM_SRC="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$WASM_SRC/.." && pwd)"
OUT_DIR="$PROJECT_ROOT/public/wasm"

mkdir -p "$OUT_DIR"

"$EMCC" -o "$OUT_DIR/games.js" \
  "$WASM_SRC/binding.c" \
  "$WASM_SRC/g2048.c" \
  "$WASM_SRC/snake.c" \
  "$WASM_SRC/sudoku.c" \
  -lm \
  --no-entry \
  -s MODULARIZE=1 \
  -s EXPORT_NAME="GameModule" \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_FUNCTIONS="['_g2048_init_js','_g2048_move_js','_g2048_tile_js','_g2048_score_js','_g2048_game_over_js','_g2048_won_js','_g2048_can_move_js','_snake_init_js','_snake_tick_js','_snake_input_js','_snake_len_js','_snake_body_x_js','_snake_body_y_js','_snake_food_x_js','_snake_food_y_js','_snake_score_js','_snake_over_js','_sudoku_init_js','_sudoku_set_js','_sudoku_erase_js','_sudoku_value_js','_sudoku_given_js','_sudoku_conflict_js','_sudoku_over_js']" \
  -O3

printf 'WASM built at %s\n' "$OUT_DIR"
