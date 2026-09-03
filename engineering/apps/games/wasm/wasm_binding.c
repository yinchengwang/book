/**
 * wasm_binding.c - Emscripten 胶水层
 *
 * 将 games_core 编译为 WebAssembly，供前端 JavaScript 直接调用。
 * 所有导出函数使用 EMSCRIPTEN_KEEPALIVE 防止被 tree-shaking 消除。
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
void* g2048_create(int seed) {
    g2048_create(&g2048_state, seed);
    return &g2048_state;
}

EMSCRIPTEN_KEEPALIVE
void g2048_move(int dir) {
    g2048_move(&g2048_state, (G2048Dir)dir);
}

/* 注意：C 函数名为 g2048_tile_at，避免与 g2048_tile 宏或重名冲突 */
EMSCRIPTEN_KEEPALIVE
int g2048_tile(int row, int col) {
    return g2048_tile_at(&g2048_state, row, col);
}

EMSCRIPTEN_KEEPALIVE
int g2048_score(void) {
    return g2048_state.score;
}

EMSCRIPTEN_KEEPALIVE
int g2048_game_over(void) {
    return g2048_state.game_over ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int g2048_won(void) {
    return g2048_state.won ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int g2048_can_move(void) {
    return g2048_can_move(&g2048_state) ? 1 : 0;
}

/* ================================================================
 * 贪吃蛇 API
 * ================================================================ */

EMSCRIPTEN_KEEPALIVE
void* snake_create(int seed, int diff) {
    snake_create(&snake_state, seed, diff);
    return &snake_state;
}

EMSCRIPTEN_KEEPALIVE
void snake_tick(void) {
    snake_tick(&snake_state);
}

EMSCRIPTEN_KEEPALIVE
void snake_input_dir(int dir) {
    snake_input(&snake_state, (SnakeDir)dir);
}

EMSCRIPTEN_KEEPALIVE
int snake_body_count(void) {
    return snake_len(&snake_state);
}

/* snake_body_x/y 与结构体字段同名，使用 _at 后缀避免符号冲突 */
EMSCRIPTEN_KEEPALIVE
int snake_body_x_at(int i) {
    return snake_body_x(&snake_state, i);
}

EMSCRIPTEN_KEEPALIVE
int snake_body_y_at(int i) {
    return snake_body_y(&snake_state, i);
}

EMSCRIPTEN_KEEPALIVE
int snake_food_x(void) {
    return snake_food_x(&snake_state);
}

EMSCRIPTEN_KEEPALIVE
int snake_food_y(void) {
    return snake_food_y(&snake_state);
}

EMSCRIPTEN_KEEPALIVE
int snake_score_val(void) {
    return snake_score(&snake_state);
}

EMSCRIPTEN_KEEPALIVE
int snake_over(void) {
    return snake_is_over(&snake_state) ? 1 : 0;
}
