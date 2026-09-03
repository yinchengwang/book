#ifndef G2048_CORE_H
#define G2048_CORE_H

#include <stdbool.h>

#define G2048_SIZE 4
#define G2048_WIN  2048

typedef enum { G2048_UP, G2048_DOWN, G2048_LEFT, G2048_RIGHT } G2048Dir;

typedef struct {
    int board[G2048_SIZE][G2048_SIZE];
    int score;
    bool moved;
    bool game_over;
    bool won;
    bool keep_going;
} G2048Game;

void  g2048_create(G2048Game *g, int seed);
void  g2048_move(G2048Game *g, G2048Dir dir);
bool  g2048_can_move(const G2048Game *g);
bool  g2048_has_won(const G2048Game *g);
int   g2048_tile_at(const G2048Game *g, int row, int col);
void  g2048_snapshot(const G2048Game *g, int out_board[G2048_SIZE][G2048_SIZE],
                     int *out_score, bool *out_game_over, bool *out_won);
#endif
