#include "g2048_core.h"
#include <stdlib.h>
#include <string.h>

#define BOARD_SIZE G2048_SIZE

/* ===== 内部辅助函数 ===== */

/* 收集所有空格坐标，随机选一个生成 2 (90%) 或 4 (10%) */
static void spawn_tile(G2048Game *g) {
    int empty[BOARD_SIZE * BOARD_SIZE][2];
    int count = 0;

    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            if (g->board[r][c] == 0) {
                empty[count][0] = r;
                empty[count][1] = c;
                count++;
            }

    if (count == 0) return;

    int idx = rand() % count;
    int r = empty[idx][0];
    int c = empty[idx][1];
    g->board[r][c] = (rand() % 10 == 0) ? 4 : 2;
}

/* 单行向左压缩+合并+再压缩，返回合并得分 */
static int slide_row(int *row) {
    int points = 0;

    /* 第一步：移除 0，向左压缩 */
    int pos = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (row[i] != 0)
            row[pos++] = row[i];
    }
    while (pos < BOARD_SIZE) row[pos++] = 0;

    /* 第二步：相邻相同合并 */
    for (int i = 0; i < BOARD_SIZE - 1; i++) {
        if (row[i] != 0 && row[i] == row[i + 1]) {
            row[i] *= 2;
            points += row[i];
            row[i + 1] = 0;
            i++; /* 跳过已合并的格子 */
        }
    }

    /* 第三步：再次压缩 */
    pos = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (row[i] != 0)
            row[pos++] = row[i];
    }
    while (pos < BOARD_SIZE) row[pos++] = 0;

    return points;
}

/* 顺时针旋转 90° */
static void rotate_cw(const G2048Game *src, G2048Game *dst) {
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            dst->board[r][c] = src->board[BOARD_SIZE - 1 - c][r];
    dst->score = src->score;
}

/* 逆时针旋转 90° */
static void rotate_ccw(const G2048Game *src, G2048Game *dst) {
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            dst->board[r][c] = src->board[c][BOARD_SIZE - 1 - r];
    dst->score = src->score;
}

/* 水平翻转 */
static void flip_h(const G2048Game *src, G2048Game *dst) {
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            dst->board[r][c] = src->board[r][BOARD_SIZE - 1 - c];
    dst->score = src->score;
}

/* ===== 四个方向移动 ===== */

static void move_left(G2048Game *g) {
    g->moved = false;

    for (int r = 0; r < BOARD_SIZE; r++) {
        int row[BOARD_SIZE];
        for (int c = 0; c < BOARD_SIZE; c++)
            row[c] = g->board[r][c];

        int points = slide_row(row);

        /* 检查是否有变化 */
        bool row_changed = false;
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (row[c] != g->board[r][c]) {
                row_changed = true;
                break;
            }
        }

        if (row_changed) {
            g->moved = true;
            g->score += points;
            for (int c = 0; c < BOARD_SIZE; c++)
                g->board[r][c] = row[c];
        }
    }
}

static void move_right(G2048Game *g) {
    G2048Game tmp;
    flip_h(g, &tmp);
    move_left(&tmp);
    flip_h(&tmp, g);
    g->moved = tmp.moved;
    g->score = tmp.score;
}

static void move_up(G2048Game *g) {
    G2048Game tmp;
    rotate_ccw(g, &tmp);
    move_left(&tmp);
    rotate_cw(&tmp, g);
    g->moved = tmp.moved;
    g->score = tmp.score;
}

static void move_down(G2048Game *g) {
    G2048Game tmp;
    rotate_cw(g, &tmp);
    move_left(&tmp);
    rotate_ccw(&tmp, g);
    g->moved = tmp.moved;
    g->score = tmp.score;
}

/* ===== 公开 API ===== */

void g2048_create(G2048Game *g, int seed) {
    memset(g, 0, sizeof(*g));
    srand((unsigned)seed);
    /* 生成初始方块（默认 2 个） */
    spawn_tile(g);
    spawn_tile(g);
}

void g2048_move(G2048Game *g, G2048Dir dir) {
    switch (dir) {
        case G2048_LEFT:
            move_left(g);
            break;
        case G2048_RIGHT:
            move_right(g);
            break;
        case G2048_UP:
            move_up(g);
            break;
        case G2048_DOWN:
            move_down(g);
            break;
    }
}

bool g2048_can_move(const G2048Game *g) {
    /* 有空位就能移 */
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (g->board[r][c] == 0) return true;
            /* 检查右邻 */
            if (c < BOARD_SIZE - 1 && g->board[r][c] == g->board[r][c + 1]) return true;
            /* 检查下邻 */
            if (r < BOARD_SIZE - 1 && g->board[r][c] == g->board[r + 1][c]) return true;
        }
    return false;
}

bool g2048_has_won(const G2048Game *g) {
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            if (g->board[r][c] >= G2048_WIN) return true;
    return false;
}

int g2048_tile_at(const G2048Game *g, int row, int col) {
    return g->board[row][col];
}

void g2048_snapshot(const G2048Game *g, int out_board[G2048_SIZE][G2048_SIZE],
                    int *out_score, bool *out_game_over, bool *out_won) {
    memcpy(out_board, g->board, sizeof(g->board));
    if (out_score)     *out_score     = g->score;
    if (out_game_over)  *out_game_over  = g->game_over;
    if (out_won)        *out_won        = g->won;
}
