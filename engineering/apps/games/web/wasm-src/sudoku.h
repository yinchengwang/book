#ifndef SUDOKU_H_INCLUDED
#define SUDOKU_H_INCLUDED

#include <stdbool.h>

/* 数独棋盘恒为 9×9。include guard 使用 SUDOKU_H_INCLUDED 形式，
 * 与未来宏 SUDOKU_H 区分。 */
#define SUDOKU_SIZE 9

typedef struct {
  int  value;    /* 0 = 空, 1..9 = 已填 */
  bool given;    /* 题目初始给定格（不可 set/erase） */
  bool conflict; /* 是否与同行/列/宫其它格冲突（渲染用） */
} SudokuCell;

typedef struct {
  SudokuCell board[SUDOKU_SIZE][SUDOKU_SIZE];
  int        solution[SUDOKU_SIZE][SUDOKU_SIZE];
  int        difficulty; /* 0=简单(30 洞) 1=中等(40 洞) 2=困难(50 洞) */
  bool       over;       /* 全部填完且无冲突时为 true */
} SudokuGame;

/* 初始化一局游戏。seed 用于 srand 保证可复现。
 * 难度决定挖洞数：0->30, 1->40, 2->50；挖洞时验证仍唯一解。 */
void sudoku_init(SudokuGame *g, int difficulty, int seed);

/* 在 (row,col) 填入 num。给定格直接拒绝。返回 1=成功 0=拒绝。 */
int  sudoku_set(SudokuGame *g, int row, int col, int num);

/* 清除 (row,col)（仅给定格和越界被忽略）。 */
void sudoku_erase(SudokuGame *g, int row, int col);

/* 返回 (row,col) 处的解（hint 用）。 */
int  sudoku_hint(const SudokuGame *g, int row, int col);

/* 在 (row,col) 填入 num 是否合法（行/列/宫无重复）。当前 value 不影响结果。 */
bool sudoku_is_valid(const SudokuGame *g, int row, int col, int num);

#endif /* SUDOKU_H_INCLUDED */
