#ifndef SNAKE_CORE_H
#define SNAKE_CORE_H

#include <stdbool.h>

#define SNAKE_MAX_LEN 200
#define SNAKE_WIDTH   20
#define SNAKE_HEIGHT  20

typedef enum { SNAKE_UP, SNAKE_DOWN, SNAKE_LEFT, SNAKE_RIGHT } SnakeDir;

typedef struct {
    int x, y;
} SnakePoint;

typedef struct {
    SnakePoint body[SNAKE_MAX_LEN];
    int        len;
    SnakePoint food;
    int        score;
    int        speed;
    bool       game_over;
    SnakeDir   dir;
    SnakeDir   next_dir;
} SnakeGame;

void snake_create(SnakeGame *g, int seed, int difficulty);
void snake_tick(SnakeGame *g);
void snake_input(SnakeGame *g, SnakeDir dir);
bool snake_is_over(const SnakeGame *g);
int  snake_score(const SnakeGame *g);
int  snake_len(const SnakeGame *g);
int  snake_food_x(const SnakeGame *g);
int  snake_food_y(const SnakeGame *g);
int  snake_body_x(const SnakeGame *g, int i);
int  snake_body_y(const SnakeGame *g, int i);
#endif
