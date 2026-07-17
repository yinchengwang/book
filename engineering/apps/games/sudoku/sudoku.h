#ifndef SUDOKU_H
#define SUDOKU_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#endif

/* 棋盘常量 */
#define SUDOKU_BOARD_SIZE 9
#define SUDOKU_BOX_SIZE  3

/* 宏别名（兼容 cpp 文件中的使用） */
#ifndef BOARD_SIZE
#define BOARD_SIZE SUDOKU_BOARD_SIZE
#endif
#ifndef BOX_SIZE
#define BOX_SIZE SUDOKU_BOX_SIZE
#endif

/* 单元格 */
struct Cell {
    int  value;       /* 0=空, 1-9=已填 */
    bool is_given;    /* 题目固定格（不可修改） */
    bool is_conflict; /* 冲突标记（渲染用） */
};

/* 游戏状态 */
struct GameState {
    Cell board[BOARD_SIZE][BOARD_SIZE];       /* 玩家棋盘 */
    Cell solution[BOARD_SIZE][BOARD_SIZE];    /* 完整解 */
    int  cursor_row, cursor_col;              /* 光标位置 (0-8) */
    int  difficulty;                          /* 0=简单, 1=中等, 2=困难 */
    int  empty_count;                         /* 剩余空格数 */
    bool game_over;                           /* 胜利标志 */
};

/* —— 平台适配 —— */
void set_utf8(void);
void console_raw_mode(void);
void console_restore(void);
void sleep_ms(int ms);
int  my_getch(void);
int  my_kbhit(void);

/* —— 核心算法 —— */
bool is_valid_placement(const Cell board[BOARD_SIZE][BOARD_SIZE], int row, int col, int num);
bool solve_sudoku(Cell board[BOARD_SIZE][BOARD_SIZE]);
int  count_solutions(Cell board[BOARD_SIZE][BOARD_SIZE], int limit);
void generate_full_board(Cell board[BOARD_SIZE][BOARD_SIZE]);
void generate_puzzle(GameState &state, int holes);

/* —— 游戏逻辑 —— */
void init_game(GameState &state, int difficulty);
void place_number(GameState &state, int num);
void erase_cell(GameState &state);
void update_conflicts(GameState &state);
bool check_win(const GameState &state);

/* —— 渲染 —— */
void render(const GameState &state);
void render_board(const GameState &state);
void render_status(const GameState &state);

/* 难度配置 */
struct DifficultyConfig {
    const char *name;
    int         holes;
};

extern const DifficultyConfig DIFFICULTIES[];

#endif /* SUDOKU_H */
