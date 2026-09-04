#include <emscripten/emscripten.h>

#include "g2048.h"

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
