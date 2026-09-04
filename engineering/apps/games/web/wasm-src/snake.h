#ifndef SNAKE_H_INCLUDED
#define SNAKE_H_INCLUDED

#include <stdbool.h>

/* 棋盘尺寸（格子数）。渲染端 canvas 400x400，格子 20px。
 * 注意：include guard 使用 SNAKE_H_INCLUDED，避免与高度宏 SNAKE_H 冲突。 */
#define SNAKE_W   20
#define SNAKE_H   20
#define SNAKE_MAX 200

typedef enum { SNAKE_UP, SNAKE_DOWN, SNAKE_LEFT, SNAKE_RIGHT } SnakeDir;

typedef struct {
    int x, y;
} Pt;

typedef struct {
    Pt       body[SNAKE_MAX]; /* body[0] 为蛇头 */
    int      len;
    Pt       food;
    int      score;
    bool     over;
    SnakeDir dir;             /* 当前生效方向 */
    SnakeDir next;            /* 下一 tick 生效的方向 */
} SnakeGame;

/* 初始化一局游戏。difficulty 仅影响调用方的 tick 间隔，逻辑层不使用。 */
void snake_init(SnakeGame *g, int seed, int difficulty);

/* 推进一帧：转向 → 移动 → 撞墙/自咬检测 → 吃食物。 */
void snake_tick(SnakeGame *g);

/* 缓冲一次转向输入（拒绝 180 度掉头）。 */
void snake_input(SnakeGame *g, SnakeDir d);

#endif /* SNAKE_H_INCLUDED */
