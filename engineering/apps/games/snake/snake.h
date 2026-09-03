#ifndef SNAKE_H
#define SNAKE_H

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

/* 游戏区尺寸（不含边框） */
#define WIDTH  100
#define HEIGHT 50
#define INIT_SNAKE_LEN 3
#define FOOD_SCORE 5

#define CLEAR_SCREEN printf("\033[2J\033[H")

/* 方向 */
enum Dir { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };
/* 难度 */
enum Diff { DIFF_EASY, DIFF_HARD, DIFF_HELL };

/* 坐标 */
typedef struct { int x, y; } Point;

/* —— 供 main.c 直接访问的全局状态 —— */
extern int game_over;
extern int score;
extern int speed;
extern int base_speed;
extern int diff;
extern enum Dir next_dir;

/* —— 公共接口 —— */
void set_utf8(void);
void console_raw_mode(void);
void console_restore(void);
void init_game(void);
void spawn_food(void);
void update(void);
void render(void);
void sleep_ms(int ms);
int  my_getch(void);
int  my_kbhit(void);

#endif /* SNAKE_H */
