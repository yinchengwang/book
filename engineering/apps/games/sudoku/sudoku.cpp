#include "sudoku.h"

/* 宏别名，确保代码兼容 */
#define BOARD_SIZE SUDOKU_BOARD_SIZE
#define BOX_SIZE SUDOKU_BOX_SIZE

/* ========== 难度配置 ========== */
const DifficultyConfig DIFFICULTIES[] = {
    { "简单", 30 },
    { "中等", 40 },
    { "困难", 50 },
};

/* ========== 平台适配（复用 snake 项目模式） ========== */

#ifdef _WIN32
static HANDLE hConsole;
static HANDLE hOutput;
static DWORD  oldMode;
static DWORD  oldOutputMode;
#endif

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

/* ========== 核心算法 ========== */

/* 检查在 (row,col) 填入 num 是否合法 */
bool is_valid_placement(const Cell board[BOARD_SIZE][BOARD_SIZE],
                        int row, int col, int num) {
    /* 行检查 */
    for (int c = 0; c < BOARD_SIZE; c++) {
        if (c != col && board[row][c].value == num) return false;
    }
    /* 列检查 */
    for (int r = 0; r < BOARD_SIZE; r++) {
        if (r != row && board[r][col].value == num) return false;
    }
    /* 3×3 宫检查 */
    int box_r = row / BOX_SIZE * BOX_SIZE;
    int box_c = col / BOX_SIZE * BOX_SIZE;
    for (int r = box_r; r < box_r + BOX_SIZE; r++) {
        for (int c = box_c; c < box_c + BOX_SIZE; c++) {
            if ((r != row || c != col) && board[r][c].value == num)
                return false;
        }
    }
    return true;
}

/* 回溯求解器：找到第一个空格，依次尝试 1-9 */
bool solve_sudoku(Cell board[BOARD_SIZE][BOARD_SIZE]) {
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board[r][c].value == 0) {
                for (int num = 1; num <= 9; num++) {
                    if (is_valid_placement(board, r, c, num)) {
                        board[r][c].value = num;
                        if (solve_sudoku(board)) return true;
                        board[r][c].value = 0;
                    }
                }
                return false; /* 1-9 都不行，回溯 */
            }
        }
    }
    return true; /* 没有空格了，求解成功 */
}

/* 计数求解器：统计解的数量，达到 limit 提前返回 */
int count_solutions(Cell board[BOARD_SIZE][BOARD_SIZE], int limit) {
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board[r][c].value == 0) {
                int count = 0;
                for (int num = 1; num <= 9; num++) {
                    if (is_valid_placement(board, r, c, num)) {
                        board[r][c].value = num;
                        count += count_solutions(board, limit);
                        board[r][c].value = 0;
                        if (count >= limit) return count;
                    }
                }
                return count;
            }
        }
    }
    return 1; /* 没有空格了，找到 1 个解 */
}

/* Fisher-Yates 洗牌 */
static void shuffle(int arr[], int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

/* 递归辅助：从 (row,col) 开始填充，候选数随机打乱 */
static bool fill_board_recursive(Cell board[BOARD_SIZE][BOARD_SIZE], int row, int col) {
    if (row == BOARD_SIZE) return true;                  /* 全部填完 */
    if (col == BOARD_SIZE) return fill_board_recursive(board, row + 1, 0);

    int nums[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    shuffle(nums, 9);

    for (int i = 0; i < 9; i++) {
        if (is_valid_placement(board, row, col, nums[i])) {
            board[row][col].value = nums[i];
            if (fill_board_recursive(board, row, col + 1)) return true;
            board[row][col].value = 0;                   /* 回溯 */
        }
    }
    return false;
}

/* 生成完整终盘（回溯 + 随机候选数顺序） */
void generate_full_board(Cell board[BOARD_SIZE][BOARD_SIZE]) {
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            board[r][c].value = 0;

    fill_board_recursive(board, 0, 0);
}

/* 挖洞法生成题目 */
void generate_puzzle(GameState &state, int holes) {
    /* 1. 生成完整终盘作为解 */
    generate_full_board(state.solution);

    /* 2. 复制到玩家棋盘 */
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            state.board[r][c].value     = state.solution[r][c].value;
            state.board[r][c].is_given  = true;
            state.board[r][c].is_conflict = false;
        }
    }

    /* 3. 随机挖洞 */
    /* 创建坐标数组并打乱 */
    int positions[81][2];
    for (int i = 0; i < 81; i++) {
        positions[i][0] = i / BOARD_SIZE;
        positions[i][1] = i % BOARD_SIZE;
    }
    /* Fisher-Yates 打乱坐标 */
    for (int i = 80; i > 0; i--) {
        int j = rand() % (i + 1);
        int tr = positions[i][0], tc = positions[i][1];
        positions[i][0] = positions[j][0];
        positions[i][1] = positions[j][1];
        positions[j][0] = tr;
        positions[j][1] = tc;
    }

    int dug = 0;
    for (int i = 0; i < 81 && dug < holes; i++) {
        int r = positions[i][0];
        int c = positions[i][1];
        int backup = state.board[r][c].value;

        state.board[r][c].value = 0;

        /* 验证仍有唯一解 */
        Cell temp[BOARD_SIZE][BOARD_SIZE];
        for (int rr = 0; rr < BOARD_SIZE; rr++)
            for (int cc = 0; cc < BOARD_SIZE; cc++)
                temp[rr][cc].value = state.board[rr][cc].value;

        if (count_solutions(temp, 2) == 1) {
            /* 唯一解，保留挖洞 */
            state.board[r][c].is_given = false;
            dug++;
        } else {
            /* 多解或无解，恢复 */
            state.board[r][c].value = backup;
        }
    }

    /* 更新空格计数 */
    state.empty_count = 0;
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            if (state.board[r][c].value == 0)
                state.empty_count++;
}

/* ========== 游戏逻辑 ========== */

void init_game(GameState &state, int difficulty) {
    memset(&state, 0, sizeof(state));
    state.difficulty   = difficulty;
    state.cursor_row   = 4;
    state.cursor_col   = 4;
    state.game_over    = false;

    int holes = DIFFICULTIES[difficulty].holes;
    generate_puzzle(state, holes);

    /* solution 不需要 is_given 标记，仅 value 有效即可 */
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            state.solution[r][c].is_given = false;
}

void place_number(GameState &state, int num) {
    int r = state.cursor_row;
    int c = state.cursor_col;

    /* 给定格不可修改 */
    if (state.board[r][c].is_given) return;

    /* 填入相同数字视为无操作 */
    if (state.board[r][c].value == num) return;

    /* 更新空格计数 */
    if (state.board[r][c].value == 0 && num != 0)
        state.empty_count--;
    else if (state.board[r][c].value != 0 && num == 0)
        state.empty_count++;

    state.board[r][c].value = num;

    /* 更新冲突标记 */
    update_conflicts(state);

    /* 检查胜利 */
    if (check_win(state)) {
        state.game_over = true;
    }
}

void erase_cell(GameState &state) {
    place_number(state, 0);
}

void update_conflicts(GameState &state) {
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (state.board[r][c].value == 0) {
                state.board[r][c].is_conflict = false;
                continue;
            }
            state.board[r][c].is_conflict =
                !is_valid_placement(state.board, r, c, state.board[r][c].value);
        }
    }
}

bool check_win(const GameState &state) {
    /* 还有空格则未完成 */
    if (state.empty_count > 0) return false;

    /* 检查所有格子无冲突 */
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            if (state.board[r][c].value == 0 || state.board[r][c].is_conflict)
                return false;

    return true;
}

/* ========== 渲染 ========== */

#define COLOR_RESET "\033[0m"

/* 获取格子的显示颜色码 */
static const char *cell_color(const GameState &state, int r, int c) {
    bool is_cursor = (r == state.cursor_row && c == state.cursor_col);

    if (state.board[r][c].is_conflict) {
        return is_cursor ? "\033[41;37m" : "\033[31m";
    }
    if (state.board[r][c].is_given) {
        return is_cursor ? "\033[44;1;37m" : "\033[1;37m";
    }
    if (state.board[r][c].value != 0) {
        return is_cursor ? "\033[44;1;36m" : "\033[36m";
    }
    return is_cursor ? "\033[44m" : "\033[0m";
}

void render_board(const GameState &state) {
    /* 列标题 */
    printf("    1 2 3   4 5 6   7 8 9\n");
    /* 顶部边框 */
    printf("  ┌───────┬───────┬───────┐\n");

    for (int r = 0; r < BOARD_SIZE; r++) {
        if (r > 0 && r % BOX_SIZE == 0)
            printf("  ├───────┼───────┼───────┤\n");

        printf("%d │", r + 1);
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (c > 0 && c % BOX_SIZE == 0) printf(" │");

            const char *color = cell_color(state, r, c);
            if (state.board[r][c].value == 0)
                printf(" %s·" COLOR_RESET, color);
            else
                printf(" %s%d" COLOR_RESET, color, state.board[r][c].value);

            if (c < BOARD_SIZE - 1) printf(" ");
        }
        printf(" │\n");
    }

    printf("  └───────┴───────┴───────┘\n");
}

void render_status(const GameState &state) {
    printf("\n  剩余空格: %d\n", state.empty_count);
    printf("  方向键:移动  |  1-9:填入  |  0/Del:擦除  |  R:重新开始  |  Q:退出\n");
}

void render(const GameState &state) {
    printf("\033[H\033[J"); /* 回左上角并清至屏尾 */
    printf("  数独 SUDOKU - %s\n\n", DIFFICULTIES[state.difficulty].name);
    render_board(state);
    render_status(state);
    fflush(stdout);
}
