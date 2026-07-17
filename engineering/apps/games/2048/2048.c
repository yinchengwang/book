#include "2048.h"

/* 宏别名，确保代码兼容 */
#define BOARD_SIZE GAME2048_BOARD_SIZE

/* ===== 平台适配 ===== */

#ifdef _WIN32
static HANDLE hConsoleIn;
static HANDLE hConsoleOut;
static DWORD oldInMode;
static DWORD oldOutMode;
#endif

void set_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

void console_raw_mode(void) {
#ifdef _WIN32
    hConsoleIn  = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hConsoleIn, &oldInMode);
    SetConsoleMode(hConsoleIn, oldInMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));

    hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hConsoleOut, &oldOutMode);
    DWORD mode = oldOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hConsoleOut, mode);
#endif
}

void console_restore(void) {
#ifdef _WIN32
    SetConsoleMode(hConsoleIn, oldInMode);
    SetConsoleMode(hConsoleOut, oldOutMode);
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
    newt.c_lflag &= (unsigned int)(~(ICANON | ECHO));
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

/* 增量渲染状态（static，跨 render 调用保持） */
static int  prev_board[BOARD_SIZE][BOARD_SIZE];
static bool first_draw = true;

/* ===== 游戏逻辑 ===== */

void init_game(struct Game2048 *g, int initial_tiles) {
    memset(g->board, 0, sizeof(g->board));
    g->score      = 0;
    g->game_over  = false;
    g->won        = false;
    g->keep_going = false;
    first_draw    = true;

    for (int i = 0; i < initial_tiles; i++)
        spawn_tile(g);
}

/* 收集所有空格坐标，随机选一个生成 2 (90%) 或 4 (10%) */
void spawn_tile(struct Game2048 *g) {
    int empty[BOARD_SIZE * BOARD_SIZE][2];
    int count = 0;

    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            if (g->board[r][c] == 0) {
                empty[count][0] = r;
                empty[count][1] = c;
                count++;
            }

    if (count == 0) return;

    int idx = rand() % count;
    int r = empty[idx][0];
    int c = empty[idx][1];
    g->board[r][c] = (rand() % 10 == 0) ? 4 : 2;
}

/* —— 向左移动（单行压缩+合并+再压缩），返回合并得分 —— */
static int slide_row(int *row) {
    int points = 0;

    /* 第一步：移除所有 0，向左压缩 */
    int pos = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (row[i] != 0)
            row[pos++] = row[i];
    }
    while (pos < BOARD_SIZE) row[pos++] = 0;

    /* 第二步：相邻相同合并 */
    for (int i = 0; i < BOARD_SIZE - 1; i++) {
        if (row[i] != 0 && row[i] == row[i + 1]) {
            row[i] *= 2;
            points += row[i];
            row[i + 1] = 0;
            i++; /* 跳过已合并的格子 */
        }
    }

    /* 第三步：再次压缩 */
    pos = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (row[i] != 0)
            row[pos++] = row[i];
    }
    while (pos < BOARD_SIZE) row[pos++] = 0;

    return points;
}

void move_left(struct Game2048 *g) {
    g->moved = false;

    for (int r = 0; r < BOARD_SIZE; r++) {
        int row[BOARD_SIZE];
        for (int c = 0; c < BOARD_SIZE; c++)
            row[c] = g->board[r][c];

        int points = slide_row(row);

        /* 检查是否有变化 */
        bool row_changed = false;
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (row[c] != g->board[r][c]) {
                row_changed = true;
                break;
            }
        }

        if (row_changed) {
            g->moved = true;
            g->score += points;
            for (int c = 0; c < BOARD_SIZE; c++)
                g->board[r][c] = row[c];
        }
    }
}

/* —— 旋转/翻转变换实现其他方向 —— */

/* 顺时针旋转 90° */
static void rotate_cw(const struct Game2048 *src, struct Game2048 *dst) {
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            dst->board[r][c] = src->board[BOARD_SIZE - 1 - c][r];
    dst->score = src->score;
}

/* 逆时针旋转 90° */
static void rotate_ccw(const struct Game2048 *src, struct Game2048 *dst) {
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            dst->board[r][c] = src->board[c][BOARD_SIZE - 1 - r];
    dst->score = src->score;
}

/* 水平翻转 */
static void flip_h(const struct Game2048 *src, struct Game2048 *dst) {
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            dst->board[r][c] = src->board[r][BOARD_SIZE - 1 - c];
    dst->score = src->score;
}

/*
 * 思路：所有方向通过旋转/翻转变换到"向左移动"，移完再变回来。
 *
 *   上移 = 逆时针转 90° → 左移 → 顺时针转 90°
 *   下移 = 顺时针转 90° → 左移 → 逆时针转 90°
 *   右移 = 水平翻转     → 左移 → 水平翻转
 *   左移 = 直接左移
 */

void move_right(struct Game2048 *g) {
    struct Game2048 tmp;
    flip_h(g, &tmp);
    move_left(&tmp);
    flip_h(&tmp, g);
    g->moved = tmp.moved;
    g->score = tmp.score;
}

void move_up(struct Game2048 *g) {
    struct Game2048 tmp;
    rotate_ccw(g, &tmp);
    move_left(&tmp);
    rotate_cw(&tmp, g);
    g->moved = tmp.moved;
    g->score = tmp.score;
}

void move_down(struct Game2048 *g) {
    struct Game2048 tmp;
    rotate_cw(g, &tmp);
    move_left(&tmp);
    rotate_ccw(&tmp, g);
    g->moved = tmp.moved;
    g->score = tmp.score;
}

/* —— 胜负判定 —— */

bool can_move(const struct Game2048 *g) {
    /* 有空位就能移 */
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (g->board[r][c] == 0) return true;
            /* 检查右邻 */
            if (c < BOARD_SIZE - 1 && g->board[r][c] == g->board[r][c + 1]) return true;
            /* 检查下邻 */
            if (r < BOARD_SIZE - 1 && g->board[r][c] == g->board[r + 1][c]) return true;
        }
    return false;
}

/* ===== 渲染 ===== */

/* 根据数值返回 ANSI 颜色码 */
static const char *tile_color(int val) {
    switch (val) {
        case 0:     return "\033[0m";
        case 2:     return "\033[37m";
        case 4:     return "\033[93m";
        case 8:     return "\033[33m";
        case 16:    return "\033[91m";
        case 32:    return "\033[31m";
        case 64:    return "\033[35m";
        case 128:   return "\033[94m";
        case 256:   return "\033[34m";
        case 512:   return "\033[36m";
        case 1024:  return "\033[92m";
        case 2048:  return "\033[93m";
        default:    return "\033[93m";
    }
}

/* 输出 4 字符宽的居中数字（带颜色，不含装饰边框） */
static void print_tile_cell(int val) {
    const char *color = tile_color(val);
    printf("%s", color);
    if (val == 0) {
        printf("    ");
    } else {
        char num[8];
        sprintf(num, "%d", val);
        int len = (int)strlen(num);
        int left  = (4 - len) / 2;
        int right = 4 - len - left;
        for (int i = 0; i < left;  i++) putchar(' ');
        printf("%s", num);
        for (int i = 0; i < right; i++) putchar(' ');
    }
    printf("\033[0m");
}

void render(const struct Game2048 *g) {
    if (first_draw) {
        /* 首次：清屏并绘制完整画面 */
        printf("\033[2J\033[H");

        printf("  ╔══════╦══════╦══════╦══════╗\n");
        for (int r = 0; r < BOARD_SIZE; r++) {
            printf("  ║");
            for (int c = 0; c < BOARD_SIZE; c++) {
                putchar(' ');
                print_tile_cell(g->board[r][c]);
                printf(" ║");
            }
            printf("\n");
            if (r < BOARD_SIZE - 1)
                printf("  ╠══════╬══════╬══════╬══════╣\n");
        }
        printf("  ╚══════╩══════╩══════╩══════╝\n");

        printf("\n  分数: %-6d   最高分: %-6d\n", g->score, g->best_score);
        printf("  WASD / 方向键移动  |  R 重新开始  |  Q 退出\n");

        if (g->won && !g->keep_going)
            printf("\n  \033[1;93m已达成 2048！按任意方向键继续游戏。\033[0m\n");

        first_draw = false;
        memcpy(prev_board, g->board, sizeof(prev_board));
        fflush(stdout);
        return;
    }

    /* 增量渲染：仅重绘变化的格子 + 更新分数行 */
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (g->board[r][c] != prev_board[r][c]) {
                /* 定位到格子内容区：
                   行 = 2 + r*2 (1-indexed)
                   列 = 5 + c*7 (1-indexed，跳过 "  ║" 和空格) */
                printf("\033[%d;%dH", 2 + r * 2, 5 + c * 7);
                print_tile_cell(g->board[r][c]);
            }
        }
    }

    /* 更新分数 */
    printf("\033[11;1H\033[K  分数: %-6d   最高分: %-6d",
           g->score, g->best_score);

    memcpy(prev_board, g->board, sizeof(prev_board));
    fflush(stdout);
}

void render_win_overlay(void) {
    printf("\n  \033[1;93m你赢了！\033[0m\n");
}

void render_lose_overlay(void) {
    printf("\n  \033[1;31m游戏结束！没有可用的移动了。\033[0m\n");
}
