#include "snake_core.h"
#include <stdlib.h>
#include <string.h>

/* —— 内部辅助 —— */

/**
 * 检查指定坐标是否在蛇身上
 * @param g 游戏状态
 * @param x 坐标 x
 * @param y 坐标 y
 * @return 1=在蛇身上，0=不在
 */
static int is_snake_pos(const SnakeGame *g, int x, int y) {
    for (int i = 0; i < g->len; i++) {
        if (g->body[i].x == x && g->body[i].y == y) {
            return 1;
        }
    }
    return 0;
}

/**
 * 生成食物（不能生成在蛇身上）
 * 使用 200 次随机尝试，失败则线性扫描全图
 */
static void spawn_food(SnakeGame *g) {
    /* 200 次随机尝试 */
    for (int i = 0; i < 200; i++) {
        g->food.x = rand() % (SNAKE_WIDTH - 2) + 1;
        g->food.y = rand() % (SNAKE_HEIGHT - 2) + 1;
        if (!is_snake_pos(g, g->food.x, g->food.y)) {
            return;
        }
    }
    /* 兜底：线性扫描 */
    for (int y = 1; y < SNAKE_HEIGHT - 1; y++) {
        for (int x = 1; x < SNAKE_WIDTH - 1; x++) {
            if (!is_snake_pos(g, x, y)) {
                g->food.x = x;
                g->food.y = y;
                return;
            }
        }
    }
}

/**
 * 更新速度（根据难度和得分）
 */
static void update_speed(SnakeGame *g, int difficulty) {
    int lvl = g->score / 5;
    if (difficulty == 0) {
        g->speed = 180 - (lvl * 5 > 140 ? 140 : lvl * 5);
    } else if (difficulty == 1) {
        g->speed = 120 - (lvl * 5 > 90 ? 90 : lvl * 5);
    } else {
        g->speed = 80 - (lvl * 4 > 60 ? 60 : lvl * 4);
    }
    if (g->speed < 20) {
        g->speed = 20;
    }
}

/* —— 公共接口 —— */

void snake_create(SnakeGame *g, int seed, int difficulty) {
    memset(g, 0, sizeof(*g));
    srand(seed);

    /* 初始化蛇身（3节，横向排列，随机位置） */
    g->len = 3;
    int sx = rand() % (SNAKE_WIDTH - 6) + 3;
    int sy = rand() % (SNAKE_HEIGHT - 6) + 3;
    for (int i = 0; i < g->len; i++) {
        g->body[i].x = sx - i;
        g->body[i].y = sy;
    }

    /* 设置初始方向 */
    g->dir = SNAKE_RIGHT;
    g->next_dir = SNAKE_RIGHT;

    /* 设置初始速度（根据难度） */
    if (difficulty == 0) {
        g->speed = 180;  /* 简单 */
    } else if (difficulty == 1) {
        g->speed = 120;  /* 困难 */
    } else {
        g->speed = 80;   /* 地狱 */
    }

    /* 生成食物 */
    spawn_food(g);
}

void snake_input(SnakeGame *g, SnakeDir dir) {
    /* 反转方向拒绝：不能直接掉头 */
    if ((dir == SNAKE_UP    && g->dir == SNAKE_DOWN)  ||
        (dir == SNAKE_DOWN  && g->dir == SNAKE_UP)    ||
        (dir == SNAKE_LEFT  && g->dir == SNAKE_RIGHT) ||
        (dir == SNAKE_RIGHT && g->dir == SNAKE_LEFT)) {
        return;
    }
    /* 只设置 next_dir，不直接改变 dir */
    g->next_dir = dir;
}

void snake_tick(SnakeGame *g) {
    /* 1. 应用 next_dir（反转检测在 input 中已做） */
    if ((g->next_dir == SNAKE_UP    && g->dir != SNAKE_DOWN)  ||
        (g->next_dir == SNAKE_DOWN  && g->dir != SNAKE_UP)    ||
        (g->next_dir == SNAKE_LEFT  && g->dir != SNAKE_RIGHT) ||
        (g->next_dir == SNAKE_RIGHT && g->dir != SNAKE_LEFT)) {
        g->dir = g->next_dir;
    }

    /* 2. 记录尾部和长度（用于后续扩展） */
    /* 注：纯逻辑实现中，吃食物后直接 len++，无需记录 prev_tail */
    (void)g->len; /* 保留此行以备将来扩展 */

    /* 3. 计算新头部位置 */
    SnakePoint head = g->body[0];
    switch (g->dir) {
        case SNAKE_UP:    head.y--; break;
        case SNAKE_DOWN:  head.y++; break;
        case SNAKE_LEFT:  head.x--; break;
        case SNAKE_RIGHT: head.x++; break;
    }

    /* 4. 撞墙检测 */
    if (head.x < 1 || head.x >= SNAKE_WIDTH - 1 ||
        head.y < 1 || head.y >= SNAKE_HEIGHT - 1) {
        g->game_over = true;
        return;
    }

    /* 5. 自咬检测 */
    for (int i = 0; i < g->len; i++) {
        if (g->body[i].x == head.x && g->body[i].y == head.y) {
            g->game_over = true;
            return;
        }
    }

    /* 6. 吃食物检测 */
    if (head.x == g->food.x && head.y == g->food.y) {
        g->score += 5;
        g->len++;
        update_speed(g, 0);  /* difficulty 参数暂用 0，实际游戏中从外部保存 */
        spawn_food(g);
    }

    /* 7. 移动蛇身（整体后移） */
    for (int i = g->len - 1; i > 0; i--) {
        g->body[i] = g->body[i - 1];
    }
    g->body[0] = head;
}

/* —— getter 函数 —— */

bool snake_is_over(const SnakeGame *g) {
    return g->game_over;
}

int snake_score(const SnakeGame *g) {
    return g->score;
}

int snake_len(const SnakeGame *g) {
    return g->len;
}

int snake_food_x(const SnakeGame *g) {
    return g->food.x;
}

int snake_food_y(const SnakeGame *g) {
    return g->food.y;
}

int snake_body_x(const SnakeGame *g, int i) {
    return g->body[i].x;
}

int snake_body_y(const SnakeGame *g, int i) {
    return g->body[i].y;
}
