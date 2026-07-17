#ifndef GAME_2048_H
#define GAME_2048_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#endif

/* 棋盘常量 */
#define GAME2048_BOARD_SIZE 4
#define WIN_VALUE  2048

/* 难度：初始生成格子数 */
#define SINGLE_INIT_TILES 1
#define EASY_INIT_TILES   2
#define HARD_INIT_TILES   3

/* 宏别名（兼容 cpp 文件中的使用） */
#ifndef BOARD_SIZE
#define BOARD_SIZE GAME2048_BOARD_SIZE
#endif

/* 游戏状态 */
struct Game2048 {
    int  board[GAME2048_BOARD_SIZE][GAME2048_BOARD_SIZE]; /* 0 = 空格 */
    int  score;
    int  best_score;
    bool moved;      /* 本轮是否发生过有效移动 */
    bool game_over;  /* 无可用移动 */
    bool won;        /* 已达成 2048，仅提示一次 */
    bool keep_going; /* 达成 2048 后继续游戏 */
};

/* —— 平台适配 —— */
void set_utf8(void);
void console_raw_mode(void);
void console_restore(void);
void sleep_ms(int ms);
int  my_getch(void);
int  my_kbhit(void);

/* —— 游戏逻辑 —— */
void init_game(struct Game2048 *g, int initial_tiles);
void spawn_tile(struct Game2048 *g);
bool can_move(const struct Game2048 *g);
void move_left(struct Game2048 *g);
void move_right(struct Game2048 *g);
void move_up(struct Game2048 *g);
void move_down(struct Game2048 *g);

/* —— 渲染 —— */
void render(const struct Game2048 *g);
void render_win_overlay(void);
void render_lose_overlay(void);

#endif /* GAME_2048_H */
