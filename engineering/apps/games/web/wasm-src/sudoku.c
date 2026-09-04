#include "sudoku.h"

#include <stdlib.h>
#include <string.h>

/* —— 难度 -> 挖洞数 —— */
static const int HOLES_BY_DIFFICULTY[3] = { 30, 40, 50 };

/* —— 内部：纯整数棋盘的回溯/校验 —— */

/* 在 (row,col) 填 num 是否合法（跳过自身位置，故与棋盘当前值无关） */
static bool valid_int(const int board[9][9], int row, int col, int num) {
  for (int c = 0; c < 9; c++) {
    if (c != col && board[row][c] == num) return false;
  }
  for (int r = 0; r < 9; r++) {
    if (r != row && board[r][col] == num) return false;
  }
  int br = (row / 3) * 3;
  int bc = (col / 3) * 3;
  for (int r = br; r < br + 3; r++) {
    for (int c = bc; c < bc + 3; c++) {
      if ((r != row || c != col) && board[r][c] == num) return false;
    }
  }
  return true;
}

/* 回溯求解（找到一组解即返回 true） */
static bool solve(int board[9][9]) {
  for (int r = 0; r < 9; r++) {
    for (int c = 0; c < 9; c++) {
      if (board[r][c] == 0) {
        for (int num = 1; num <= 9; num++) {
          if (valid_int(board, r, c, num)) {
            board[r][c] = num;
            if (solve(board)) return true;
            board[r][c] = 0;
          }
        }
        return false;
      }
    }
  }
  return true;
}

/* 解的计数（达到 limit 立即返回，用于唯一性校验） */
static int count_solutions(int board[9][9], int limit) {
  for (int r = 0; r < 9; r++) {
    for (int c = 0; c < 9; c++) {
      if (board[r][c] == 0) {
        int count = 0;
        for (int num = 1; num <= 9; num++) {
          if (valid_int(board, r, c, num)) {
            board[r][c] = num;
            count += count_solutions(board, limit);
            board[r][c] = 0;
            if (count >= limit) return count;
          }
        }
        return count;
      }
    }
  }
  return 1;
}

/* Fisher-Yates 洗牌 */
static void shuffle(int *arr, int n) {
  for (int i = n - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    int t = arr[i];
    arr[i] = arr[j];
    arr[j] = t;
  }
}

/* 从 (row,col) 起递归填满，候选数 1..9 随机打乱 */
static bool fill_rec(int board[9][9], int row, int col) {
  if (row == 9) return true;
  if (col == 9) return fill_rec(board, row + 1, 0);
  int nums[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  shuffle(nums, 9);
  for (int i = 0; i < 9; i++) {
    if (valid_int(board, row, col, nums[i])) {
      board[row][col] = nums[i];
      if (fill_rec(board, row, col + 1)) return true;
      board[row][col] = 0;
    }
  }
  return false;
}

/* 生成完整终盘（写入给定 9×9 整型数组） */
static void generate_full(int board[9][9]) {
  for (int r = 0; r < 9; r++)
    for (int c = 0; c < 9; c++)
      board[r][c] = 0;
  fill_rec(board, 0, 0);
}

/* —— 内部：SudokuCell 棋盘的冲突重算 / 胜利判定 —— */

/* (row,col) 处当前 value 是否与棋盘其它格冲突 */
static bool has_conflict(const SudokuGame *g, int row, int col) {
  int v = g->board[row][col].value;
  if (v == 0) return false;
  /* 行 */
  for (int c = 0; c < 9; c++) {
    if (c != col && g->board[row][c].value == v) return true;
  }
  /* 列 */
  for (int r = 0; r < 9; r++) {
    if (r != row && g->board[r][col].value == v) return true;
  }
  /* 宫 */
  int br = (row / 3) * 3;
  int bc = (col / 3) * 3;
  for (int r = br; r < br + 3; r++) {
    for (int c = bc; c < bc + 3; c++) {
      if ((r != row || c != col) && g->board[r][c].value == v) return true;
    }
  }
  return false;
}

static void update_conflicts(SudokuGame *g) {
  for (int r = 0; r < 9; r++) {
    for (int c = 0; c < 9; c++) {
      g->board[r][c].conflict = has_conflict(g, r, c);
    }
  }
}

static bool all_filled_and_clean(const SudokuGame *g) {
  for (int r = 0; r < 9; r++) {
    for (int c = 0; c < 9; c++) {
      if (g->board[r][c].value == 0) return false;
      if (g->board[r][c].conflict) return false;
    }
  }
  return true;
}

/* —— 公共接口 —— */

void sudoku_init(SudokuGame *g, int difficulty, int seed) {
  if (difficulty < 0) difficulty = 0;
  if (difficulty > 2) difficulty = 2;

  srand((unsigned int)seed);
  memset(g, 0, sizeof(*g));
  g->difficulty = difficulty;
  g->over = false;

  int holes = HOLES_BY_DIFFICULTY[difficulty];

  /* 1. 生成完整终盘 */
  int full[9][9];
  generate_full(full);

  /* 2. 写入 solution */
  for (int r = 0; r < 9; r++)
    for (int c = 0; c < 9; c++)
      g->solution[r][c] = full[r][c];

  /* 3. 玩家棋盘初始全部 given=true */
  for (int r = 0; r < 9; r++) {
    for (int c = 0; c < 9; c++) {
      g->board[r][c].value = full[r][c];
      g->board[r][c].given = true;
      g->board[r][c].conflict = false;
    }
  }

  /* 4. 打乱坐标后逐格挖洞（验证唯一解，不唯一则回填） */
  int pos[81][2];
  for (int i = 0; i < 81; i++) {
    pos[i][0] = i / 9;
    pos[i][1] = i % 9;
  }
  for (int i = 80; i > 0; i--) {
    int j = rand() % (i + 1);
    int tr = pos[i][0], tc = pos[i][1];
    pos[i][0] = pos[j][0];
    pos[i][1] = pos[j][1];
    pos[j][0] = tr;
    pos[j][1] = tc;
  }

  int dug = 0;
  for (int i = 0; i < 81 && dug < holes; i++) {
    int r = pos[i][0];
    int c = pos[i][1];
    int backup = g->board[r][c].value;

    g->board[r][c].value = 0;

    int tmp[9][9];
    for (int rr = 0; rr < 9; rr++)
      for (int cc = 0; cc < 9; cc++)
        tmp[rr][cc] = g->board[rr][cc].value;

    if (count_solutions(tmp, 2) == 1) {
      g->board[r][c].given = false;
      dug++;
    } else {
      g->board[r][c].value = backup;
    }
  }
}

int sudoku_set(SudokuGame *g, int row, int col, int num) {
  if (row < 0 || row >= 9 || col < 0 || col >= 9) return 0;
  if (g->over) return 0;
  if (g->board[row][col].given) return 0;
  if (num < 0 || num > 9) return 0;

  g->board[row][col].value = num;
  update_conflicts(g);

  if (all_filled_and_clean(g)) g->over = true;
  return 1;
}

void sudoku_erase(SudokuGame *g, int row, int col) {
  if (row < 0 || row >= 9 || col < 0 || col >= 9) return;
  if (g->over) return;
  if (g->board[row][col].given) return;

  g->board[row][col].value = 0;
  update_conflicts(g);
  g->over = false;
}

int sudoku_hint(const SudokuGame *g, int row, int col) {
  if (row < 0 || row >= 9 || col < 0 || col >= 9) return 0;
  return g->solution[row][col];
}

bool sudoku_is_valid(const SudokuGame *g, int row, int col, int num) {
  if (row < 0 || row >= 9 || col < 0 || col >= 9) return false;
  if (num < 1 || num > 9) return false;
  /* 与 valid_int 等价逻辑：检查 num 是否已在 row/col/box 的其它位置出现 */
  for (int c = 0; c < 9; c++) {
    if (c != col && g->board[row][c].value == num) return false;
  }
  for (int r = 0; r < 9; r++) {
    if (r != row && g->board[r][col].value == num) return false;
  }
  int br = (row / 3) * 3;
  int bc = (col / 3) * 3;
  for (int r = br; r < br + 3; r++) {
    for (int c = bc; c < bc + 3; c++) {
      if ((r != row || c != col) && g->board[r][c].value == num) return false;
    }
  }
  return true;
}
