#include "snake.h"

/* —— 全局状态定义 —— */
Point snake[WIDTH * HEIGHT];
int snake_len;
Point food;
enum Dir dir, next_dir;
int score;
int speed;
int base_speed;
int diff;
int game_over;
int snake_growth_pending;
Point prev_tail;
int prev_snake_len;
int first_render;

#ifdef _WIN32
static HANDLE hConsole;
static HANDLE hOutput;
static DWORD oldMode;
static DWORD oldOutputMode;
#endif

/* —— 平台适配 —— */
void set_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep((unsigned int)ms * 1000);
#endif
}

int my_getch(void) {
#ifdef _WIN32
    return _getch();
#else
    struct termios oldt, newt;
    tcgetattr(0, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &newt);
    int c = getchar();
    tcsetattr(0, TCSANOW, &oldt);
    return c;
#endif
}

int my_kbhit(void) {
#ifdef _WIN32
    return _kbhit();
#else
    struct timeval tv = {0, 0};
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(0, &readset);
    return select(1, &readset, NULL, NULL, &tv) > 0;
#endif
}

void console_raw_mode(void) {
#ifdef _WIN32
    hConsole = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hConsole, &oldMode);
    SetConsoleMode(hConsole, oldMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));

    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hOutput, &oldOutputMode);
    DWORD dwMode = oldOutputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOutput, dwMode);
#endif
}

void console_restore(void) {
#ifdef _WIN32
    SetConsoleMode(hConsole, oldMode);
    SetConsoleMode(hOutput, oldOutputMode);
#endif
}

/* —— 游戏逻辑 —— */
static int is_snake_pos(int x, int y) {
    for (int i = 0; i < snake_len; i++)
        if (snake[i].x == x && snake[i].y == y) return 1;
    return 0;
}

void spawn_food(void) {
    for (int i = 0; i < 2000; i++) {
        food.x = rand() % (WIDTH - 2) + 1;
        food.y = rand() % (HEIGHT - 2) + 1;
        if (!is_snake_pos(food.x, food.y)) return;
    }
    for (int y = 1; y < HEIGHT - 1; y++)
        for (int x = 1; x < WIDTH - 1; x++)
            if (!is_snake_pos(x, y)) { food.x = x; food.y = y; return; }
}

void init_game(void) {
    snake_len = INIT_SNAKE_LEN;
    int sx = rand() % (WIDTH - 6) + 3;
    int sy = rand() % (HEIGHT - 6) + 3;
    for (int i = 0; i < snake_len; i++) {
        snake[i].x = sx - i;
        snake[i].y = sy;
    }
    dir = DIR_RIGHT;
    next_dir = DIR_RIGHT;
    score = 0;
    speed = base_speed;
    game_over = 0;
    snake_growth_pending = 0;
    first_render = 1;
    spawn_food();
}

static void update_speed(void) {
    int lvl = score / FOOD_SCORE;
    if (diff == DIFF_EASY) {
        speed = 180 - (lvl * 5 > 140 ? 140 : lvl * 5);
    } else if (diff == DIFF_HARD) {
        speed = 120 - (lvl * 5 > 90 ? 90 : lvl * 5);
    } else {
        speed = 80 - (lvl * 4 > 60 ? 60 : lvl * 4);
    }
    if (speed < 20) speed = 20;
}

void update(void) {
    if ((next_dir == DIR_UP    && dir != DIR_DOWN)  ||
        (next_dir == DIR_DOWN  && dir != DIR_UP)    ||
        (next_dir == DIR_LEFT  && dir != DIR_RIGHT) ||
        (next_dir == DIR_RIGHT && dir != DIR_LEFT)) {
        dir = next_dir;
    }

    Point head = snake[0];
    switch (dir) {
        case DIR_UP:    head.y--; break;
        case DIR_DOWN:  head.y++; break;
        case DIR_LEFT:  head.x--; break;
        case DIR_RIGHT: head.x++; break;
    }

    if (head.x < 1 || head.x >= WIDTH - 1 || head.y < 1 || head.y >= HEIGHT - 1) {
        game_over = 1;
        return;
    }

    for (int i = 0; i < snake_len; i++) {
        if (snake[i].x == head.x && snake[i].y == head.y) {
            game_over = 1;
            return;
        }
    }

    if (head.x == food.x && head.y == food.y) {
        score += FOOD_SCORE;
        update_speed();
        snake_growth_pending = 1;
        spawn_food();
    }

    prev_tail = snake[snake_len - 1];
    prev_snake_len = snake_len;

    if (snake_growth_pending) {
        snake_len++;
        snake_growth_pending = 0;
    }

    for (int i = snake_len - 1; i > 0; i--)
        snake[i] = snake[i - 1];
    snake[0] = head;
}

void render(void) {
    if (first_render) {
        CLEAR_SCREEN;
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                if (y == 0 || y == HEIGHT - 1 || x == 0 || x == WIDTH - 1)
                    printf("#");
                else
                    printf(" ");
            }
            printf("\n");
        }

        for (int i = 0; i < snake_len; i++) {
            printf("\033[%d;%dH", snake[i].y + 1, snake[i].x + 1);
            if (i == 0) printf("\033[1;32m@\033[0m");
            else        printf("\033[0;32mo\033[0m");
        }

        printf("\033[%d;%dH\033[1;31m*\033[0m", food.y + 1, food.x + 1);

        printf("\033[%d;1H\033[K  %-8s %4d    %-8s %4d    %-8s %4d ms",
               HEIGHT + 1, "得分:", score, "长度:", snake_len, "速度:", speed);
        printf("\033[%d;1H\033[K  %-8s %s",
               HEIGHT + 2, "难度:", diff == DIFF_EASY ? "简单" : diff == DIFF_HARD ? "困难" : "地域");
        printf("\033[%d;1H\033[K  WASD/方向键移动, Q退出", HEIGHT + 3);

        first_render = 0;
        fflush(stdout);
        return;
    }

    if (snake_len == prev_snake_len) {
        printf("\033[%d;%dH ", prev_tail.y + 1, prev_tail.x + 1);
    }

    if (snake_len >= 2) {
        printf("\033[%d;%dH\033[0;32mo\033[0m", snake[1].y + 1, snake[1].x + 1);
    }

    printf("\033[%d;%dH\033[1;32m@\033[0m", snake[0].y + 1, snake[0].x + 1);
    printf("\033[%d;%dH\033[1;31m*\033[0m", food.y + 1, food.x + 1);

    printf("\033[%d;1H\033[K  %-8s %4d    %-8s %4d    %-8s %4d ms",
           HEIGHT + 1, "得分:", score, "长度:", snake_len, "速度:", speed);
    printf("\033[%d;1H\033[K  %-8s %s",
           HEIGHT + 2, "难度:", diff == DIFF_EASY ? "简单" : diff == DIFF_HARD ? "困难" : "地域");
    printf("\033[%d;1H\033[K  WASD/方向键移动, Q退出", HEIGHT + 3);

    fflush(stdout);
}
