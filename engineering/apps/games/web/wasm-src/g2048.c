#include "g2048.h"

#include <stdlib.h>
#include <string.h>

#define BOARD_SIZE G2048_SIZE
#define WIN_TILE 2048

/* Generate a 2 (90%) or 4 (10%) in a randomly selected empty cell. */
static void spawn_tile(G2048Game *g) {
    int empty[BOARD_SIZE * BOARD_SIZE][2];
    int count = 0;

    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (g->board[row][col] == 0) {
                empty[count][0] = row;
                empty[count][1] = col;
                count++;
            }
        }
    }

    if (count == 0) {
        return;
    }

    const int index = rand() % count;
    const int row = empty[index][0];
    const int col = empty[index][1];
    g->board[row][col] = (rand() % 10 == 0) ? 4 : 2;
}

/* Compact a line, merge equal adjacent tiles once, then compact again. */
static int slide_line(int *line) {
    int points = 0;
    int write = 0;

    for (int i = 0; i < BOARD_SIZE; i++) {
        if (line[i] != 0) {
            line[write++] = line[i];
        }
    }
    while (write < BOARD_SIZE) {
        line[write++] = 0;
    }

    for (int i = 0; i < BOARD_SIZE - 1; i++) {
        if (line[i] != 0 && line[i] == line[i + 1]) {
            line[i] *= 2;
            points += line[i];
            line[i + 1] = 0;
            i++;
        }
    }

    write = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (line[i] != 0) {
            line[write++] = line[i];
        }
    }
    while (write < BOARD_SIZE) {
        line[write++] = 0;
    }

    return points;
}

static int line_changed(const int *before, const int *after) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (before[i] != after[i]) {
            return 1;
        }
    }
    return 0;
}

static int move_left(G2048Game *g) {
    int moved = 0;

    for (int row = 0; row < BOARD_SIZE; row++) {
        int before[BOARD_SIZE];
        int line[BOARD_SIZE];

        for (int col = 0; col < BOARD_SIZE; col++) {
            before[col] = g->board[row][col];
            line[col] = before[col];
        }

        const int points = slide_line(line);
        if (line_changed(before, line)) {
            moved = 1;
            g->score += points;
            for (int col = 0; col < BOARD_SIZE; col++) {
                g->board[row][col] = line[col];
            }
        }
    }

    return moved;
}

static int move_right(G2048Game *g) {
    int moved = 0;

    for (int row = 0; row < BOARD_SIZE; row++) {
        int before[BOARD_SIZE];
        int line[BOARD_SIZE];

        for (int col = 0; col < BOARD_SIZE; col++) {
            before[col] = g->board[row][col];
        }
        for (int col = 0; col < BOARD_SIZE; col++) {
            line[col] = before[BOARD_SIZE - 1 - col];
        }

        const int points = slide_line(line);
        int line_moved = 0;
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (before[col] != line[BOARD_SIZE - 1 - col]) {
                line_moved = 1;
            }
            g->board[row][col] = line[BOARD_SIZE - 1 - col];
        }
        if (line_moved) {
            moved = 1;
            g->score += points;
        }
    }

    return moved;
}

static int move_up(G2048Game *g) {
    int moved = 0;

    for (int col = 0; col < BOARD_SIZE; col++) {
        int before[BOARD_SIZE];
        int line[BOARD_SIZE];

        for (int row = 0; row < BOARD_SIZE; row++) {
            before[row] = g->board[row][col];
            line[row] = before[row];
        }

        const int points = slide_line(line);
        if (line_changed(before, line)) {
            moved = 1;
            g->score += points;
            for (int row = 0; row < BOARD_SIZE; row++) {
                g->board[row][col] = line[row];
            }
        }
    }

    return moved;
}

static int move_down(G2048Game *g) {
    int moved = 0;

    for (int col = 0; col < BOARD_SIZE; col++) {
        int before[BOARD_SIZE];
        int line[BOARD_SIZE];

        for (int row = 0; row < BOARD_SIZE; row++) {
            before[row] = g->board[row][col];
        }
        for (int row = 0; row < BOARD_SIZE; row++) {
            line[row] = before[BOARD_SIZE - 1 - row];
        }

        const int points = slide_line(line);
        int line_moved = 0;
        for (int row = 0; row < BOARD_SIZE; row++) {
            if (before[row] != line[BOARD_SIZE - 1 - row]) {
                line_moved = 1;
            }
            g->board[row][col] = line[BOARD_SIZE - 1 - row];
        }
        if (line_moved) {
            moved = 1;
            g->score += points;
        }
    }

    return moved;
}

static void update_status(G2048Game *g) {
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (g->board[row][col] >= WIN_TILE) {
                g->won = true;
            }
        }
    }
    g->game_over = !g2048_can_move(g);
}

void g2048_init(G2048Game *g, int seed) {
    memset(g, 0, sizeof(*g));
    srand((unsigned)seed);
    spawn_tile(g);
    spawn_tile(g);
}

int g2048_move(G2048Game *g, G2048Dir dir) {
    if (g == NULL || g->game_over) {
        return 0;
    }

    int moved = 0;
    switch (dir) {
        case G2048_UP:
            moved = move_up(g);
            break;
        case G2048_DOWN:
            moved = move_down(g);
            break;
        case G2048_LEFT:
            moved = move_left(g);
            break;
        case G2048_RIGHT:
            moved = move_right(g);
            break;
        default:
            return 0;
    }

    if (moved) {
        spawn_tile(g);
    }
    update_status(g);
    return moved;
}

int g2048_tile_at(const G2048Game *g, int row, int col) {
    if (g == NULL || row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        return 0;
    }
    return g->board[row][col];
}

bool g2048_can_move(const G2048Game *g) {
    if (g == NULL) {
        return false;
    }

    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (g->board[row][col] == 0) {
                return true;
            }
            if (col + 1 < BOARD_SIZE && g->board[row][col] == g->board[row][col + 1]) {
                return true;
            }
            if (row + 1 < BOARD_SIZE && g->board[row][col] == g->board[row + 1][col]) {
                return true;
            }
        }
    }

    return false;
}

void g2048_set(G2048Game *g, const int tiles[16], int score) {
    if (g == NULL || tiles == NULL) {
        return;
    }
    for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        g->board[i / BOARD_SIZE][i % BOARD_SIZE] = tiles[i];
    }
    g->score = score;
    update_status(g);
}
