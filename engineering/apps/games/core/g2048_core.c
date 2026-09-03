#include "g2048_core.h"
#include <stdlib.h>
#include <string.h>

void g2048_create(G2048Game *g, int seed) {
    memset(g, 0, sizeof(*g));
    srand((unsigned)seed);
}

void g2048_move(G2048Game *g, G2048Dir dir) {
    (void)g;
    (void)dir;
}

bool g2048_can_move(const G2048Game *g) {
    (void)g;
    return false;
}

bool g2048_has_won(const G2048Game *g) {
    (void)g;
    return false;
}

int g2048_tile_at(const G2048Game *g, int row, int col) {
    (void)g;
    (void)row;
    (void)col;
    return 0;
}

void g2048_snapshot(const G2048Game *g, int out_board[G2048_SIZE][G2048_SIZE],
                    int *out_score, bool *out_game_over, bool *out_won) {
    (void)g;
    (void)out_board;
    (void)out_score;
    (void)out_game_over;
    (void)out_won;
}
