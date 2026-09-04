#include "snake.h"

#include <stdlib.h>
#include <string.h>

/* —— 内部辅助 —— */

/* 指定坐标是否被蛇身占据 */
static bool is_snake_pos(const SnakeGame *g, int x, int y) {
    for (int i = 0; i < g->len; i++) {
        if (g->body[i].x == x && g->body[i].y == y) {
            return true;
        }
    }
    return false;
}

/* 生成食物（不能落在蛇身上）：先随机尝试，失败则线性扫描兜底 */
static void spawn_food(SnakeGame *g) {
    for (int i = 0; i < 200; i++) {
        int x = rand() % SNAKE_W;
        int y = rand() % SNAKE_H;
        if (!is_snake_pos(g, x, y)) {
            g->food.x = x;
            g->food.y = y;
            return;
        }
    }
    for (int y = 0; y < SNAKE_H; y++) {
        for (int x = 0; x < SNAKE_W; x++) {
            if (!is_snake_pos(g, x, y)) {
                g->food.x = x;
                g->food.y = y;
                return;
            }
        }
    }
}

/* —— 公共接口 —— */

void snake_init(SnakeGame *g, int seed, int difficulty) {
    (void)difficulty; /* 速度由调用方（JS tick 间隔）控制 */

    memset(g, 0, sizeof(*g));
    srand((unsigned int)seed);

    /* 蛇身 3 节，横向排列，随机落点（保证整条蛇在棋盘内） */
    g->len = 3;
    int sx = rand() % (SNAKE_W - 6) + 3;
    int sy = rand() % (SNAKE_H - 6) + 3;
    for (int i = 0; i < g->len; i++) {
        g->body[i].x = sx - i;
        g->body[i].y = sy;
    }

    g->dir = SNAKE_RIGHT;
    g->next = SNAKE_RIGHT;
    g->score = 0;
    g->over = false;

    spawn_food(g);
}

void snake_input(SnakeGame *g, SnakeDir d) {
    if (g->over) {
        return;
    }
    /* 拒绝 180 度掉头 */
    if ((d == SNAKE_UP && g->dir == SNAKE_DOWN) ||
        (d == SNAKE_DOWN && g->dir == SNAKE_UP) ||
        (d == SNAKE_LEFT && g->dir == SNAKE_RIGHT) ||
        (d == SNAKE_RIGHT && g->dir == SNAKE_LEFT)) {
        return;
    }
    g->next = d;
}

void snake_tick(SnakeGame *g) {
    if (g->over) {
        return;
    }

    /* 1. 应用缓冲方向 */
    g->dir = g->next;

    /* 2. 新头部位置 */
    Pt head = g->body[0];
    switch (g->dir) {
        case SNAKE_UP:    head.y--; break;
        case SNAKE_DOWN:  head.y++; break;
        case SNAKE_LEFT:  head.x--; break;
        case SNAKE_RIGHT: head.x++; break;
    }

    /* 3. 撞墙检测（基础版：撞墙即死，无穿墙） */
    if (head.x < 0 || head.x >= SNAKE_W || head.y < 0 || head.y >= SNAKE_H) {
        g->over = true;
        return;
    }

    /* 4. 吃食物检测（先判定，决定尾巴是否让位） */
    bool ate = (head.x == g->food.x && head.y == g->food.y);

    /* 5. 自咬检测：未进食时尾节点本帧会移开，不算碰撞 */
    int check_len = ate ? g->len : g->len - 1;
    for (int i = 0; i < check_len; i++) {
        if (g->body[i].x == head.x && g->body[i].y == head.y) {
            g->over = true;
            return;
        }
    }

    /* 6. 增长 / 计分 */
    if (ate) {
        g->score += 5;
        if (g->len < SNAKE_MAX) {
            g->len++;
        }
    }

    /* 7. 蛇身整体后移 */
    for (int i = g->len - 1; i > 0; i--) {
        g->body[i] = g->body[i - 1];
    }
    g->body[0] = head;

    /* 8. 增长后再生成食物（保证不落在新蛇身上） */
    if (ate) {
        spawn_food(g);
    }
}
