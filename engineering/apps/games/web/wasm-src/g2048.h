#ifndef G2048_H
#define G2048_H

#include <stdbool.h>

#define G2048_SIZE 4

typedef enum { G2048_UP, G2048_DOWN, G2048_LEFT, G2048_RIGHT } G2048Dir;

typedef struct {
    int board[G2048_SIZE][G2048_SIZE];
    int score;
    bool game_over;
    bool won;
} G2048Game;

void g2048_init(G2048Game *g, int seed);
int g2048_move(G2048Game *g, G2048Dir dir);
int g2048_tile_at(const G2048Game *g, int row, int col);
bool g2048_can_move(const G2048Game *g);
void g2048_set(G2048Game *g, const int tiles[16], int score);

#endif
