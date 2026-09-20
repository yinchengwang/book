/**
 * wasm_binding.c - Emscripten 胶水层
 *
 * 将 games_core 编译为 WebAssembly，供前端 JavaScript 直接调用。
 */

#include <emscripten/emscripten.h>
#include "../core/g2048_core.h"
#include "../core/snake_core.h"

/* === 2048 全局状态 === */
static G2048Game g2048_state;

/* === 贪吃蛇全局状态 === */
static SnakeGame snake_state;

/* ================================================================
 * 2048 API
 * ================================================================ */

EMSCRIPTEN_KEEPALIVE
void* wasm_g2048_create(int seed) {
    g2048_create(&g2048_state, seed);
    return &g2048_state;
}

EMSCRIPTEN_KEEPALIVE
void wasm_g2048_move(int dir) {
    g2048_move(&g2048_state, (G2048Dir)dir);
}

EMSCRIPTEN_KEEPALIVE
int wasm_g2048_tile(int row, int col) {
    return g2048_tile_at(&g2048_state, row, col);
}

EMSCRIPTEN_KEEPALIVE
int wasm_g2048_score(void) {
    return g2048_state.score;
}

EMSCRIPTEN_KEEPALIVE
int wasm_g2048_game_over(void) {
    return g2048_state.game_over ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int wasm_g2048_won(void) {
    return g2048_state.won ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int wasm_g2048_can_move(void) {
    return g2048_can_move(&g2048_state) ? 1 : 0;
}

/* ================================================================
 * 贪吃蛇 API
 * ================================================================ */

EMSCRIPTEN_KEEPALIVE
void* wasm_snake_create(int seed, int diff) {
    snake_create(&snake_state, seed, diff);
    return &snake_state;
}

EMSCRIPTEN_KEEPALIVE
void wasm_snake_tick(void) {
    snake_tick(&snake_state);
}

EMSCRIPTEN_KEEPALIVE
void wasm_snake_input(int dir) {
    snake_input(&snake_state, (SnakeDir)dir);
}

EMSCRIPTEN_KEEPALIVE
int wasm_snake_body_count(void) {
    return snake_len(&snake_state);
}

EMSCRIPTEN_KEEPALIVE
int wasm_snake_body_x(int i) {
    return snake_body_x(&snake_state, i);
}

EMSCRIPTEN_KEEPALIVE
int wasm_snake_body_y(int i) {
    return snake_body_y(&snake_state, i);
}

EMSCRIPTEN_KEEPALIVE
int wasm_snake_food_x(void) {
    return snake_food_x(&snake_state);
}

EMSCRIPTEN_KEEPALIVE
int wasm_snake_food_y(void) {
    return snake_food_y(&snake_state);
}

EMSCRIPTEN_KEEPALIVE
int wasm_snake_score(void) {
    return snake_score(&snake_state);
}

EMSCRIPTEN_KEEPALIVE
int wasm_snake_over(void) {
    return snake_is_over(&snake_state) ? 1 : 0;
}
