#include <emscripten/emscripten.h>

#include "g2048.h"
#include "snake.h"
#include "sudoku.h"

static G2048Game g_state;

EMSCRIPTEN_KEEPALIVE
void g2048_init_js(int seed) {
    g2048_init(&g_state, seed);
}

EMSCRIPTEN_KEEPALIVE
int g2048_move_js(int dir) {
    return g2048_move(&g_state, (G2048Dir)dir);
}

EMSCRIPTEN_KEEPALIVE
int g2048_tile_js(int row, int col) {
    return g2048_tile_at(&g_state, row, col);
}

EMSCRIPTEN_KEEPALIVE
int g2048_score_js(void) {
    return g_state.score;
}

EMSCRIPTEN_KEEPALIVE
int g2048_game_over_js(void) {
    return g_state.game_over ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int g2048_won_js(void) {
    return g_state.won ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int g2048_can_move_js(void) {
    return g2048_can_move(&g_state) ? 1 : 0;
}

/* —— snake —— */

static SnakeGame s_state;

EMSCRIPTEN_KEEPALIVE
void snake_init_js(int seed, int diff) {
    snake_init(&s_state, seed, diff);
}

EMSCRIPTEN_KEEPALIVE
void snake_tick_js(void) {
    snake_tick(&s_state);
}

EMSCRIPTEN_KEEPALIVE
void snake_input_js(int d) {
    snake_input(&s_state, (SnakeDir)d);
}

EMSCRIPTEN_KEEPALIVE
int snake_len_js(void) {
    return s_state.len;
}

EMSCRIPTEN_KEEPALIVE
int snake_body_x_js(int i) {
    if (i < 0 || i >= s_state.len) {
        return -1;
    }
    return s_state.body[i].x;
}

EMSCRIPTEN_KEEPALIVE
int snake_body_y_js(int i) {
    if (i < 0 || i >= s_state.len) {
        return -1;
    }
    return s_state.body[i].y;
}

EMSCRIPTEN_KEEPALIVE
int snake_food_x_js(void) {
    return s_state.food.x;
}

EMSCRIPTEN_KEEPALIVE
int snake_food_y_js(void) {
    return s_state.food.y;
}

EMSCRIPTEN_KEEPALIVE
int snake_score_js(void) {
    return s_state.score;
}

EMSCRIPTEN_KEEPALIVE
int snake_over_js(void) {
    return s_state.over ? 1 : 0;
}

/* —— sudoku —— */

static SudokuGame sd_state;

EMSCRIPTEN_KEEPALIVE
void sudoku_init_js(int d, int seed) {
    sudoku_init(&sd_state, d, seed);
}

EMSCRIPTEN_KEEPALIVE
int sudoku_set_js(int r, int c, int n) {
    return sudoku_set(&sd_state, r, c, n);
}

EMSCRIPTEN_KEEPALIVE
void sudoku_erase_js(int r, int c) {
    sudoku_erase(&sd_state, r, c);
}

EMSCRIPTEN_KEEPALIVE
int sudoku_value_js(int r, int c) {
    return sd_state.board[r][c].value;
}

EMSCRIPTEN_KEEPALIVE
int sudoku_given_js(int r, int c) {
    return sd_state.board[r][c].given ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int sudoku_conflict_js(int r, int c) {
    return sd_state.board[r][c].conflict ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int sudoku_over_js(void) {
    return sd_state.over ? 1 : 0;
}
