#include "sudoku.h"

int main(void) {
    set_utf8();
    srand((unsigned)time(NULL));

    /* ===== 标题与难度选择 ===== */
    printf("\n");
    printf("  ╔══════════════════════════════════╗\n");
    printf("  ║         数独  SUDOKU             ║\n");
    printf("  ╠══════════════════════════════════╣\n");
    printf("  ║  1. 简单 (30 个空格)             ║\n");
    printf("  ║  2. 中等 (40 个空格)             ║\n");
    printf("  ║  3. 困难 (50 个空格)             ║\n");
    printf("  ║  按 Q 退出                       ║\n");
    printf("  ╚══════════════════════════════════╝\n");
    printf("  请选择难度 [1-3]: ");

    int ch = my_getch();
    printf("%c\n\n", ch);

    if (ch == 'q' || ch == 'Q') {
        printf("  再见！\n");
        return 0;
    }

    int difficulty = 0;
    if (ch == '1')      difficulty = 0;
    else if (ch == '2') difficulty = 1;
    else if (ch == '3') difficulty = 2;
    else                difficulty = 0; /* 默认简单 */

    /* ===== 进入原始输入模式 ===== */
    console_raw_mode();

    /* 初始化游戏 */
    GameState state;
    init_game(state, difficulty);

    /* 隐藏光标 */
    printf("\033[?25l");

    /* 首次渲染 */
    render(state);

    /* ===== 主游戏循环 ===== */
    while (!state.game_over) {
        if (my_kbhit()) {
            int c = my_getch();

            /* 排空残余输入（仅当非方向键前缀时安全） */
            if (c != 0xE0 && c != 0x00) {
                while (my_kbhit()) my_getch();
            }

            switch (c) {
            /* —— 方向键（Windows 双字节码 0xE0/0x00 前缀） —— */
#ifdef _WIN32
            case 0xE0:
            case 0x00: {
                int arrow = my_getch();
                switch (arrow) {
                case 72: /* 上 */ if (state.cursor_row > 0) state.cursor_row--; break;
                case 80: /* 下 */ if (state.cursor_row < 8) state.cursor_row++; break;
                case 75: /* 左 */ if (state.cursor_col > 0) state.cursor_col--; break;
                case 77: /* 右 */ if (state.cursor_col < 8) state.cursor_col++; break;
                }
                break;
            }
#else
            case '\033': {
                int b = my_getch();
                if (b == '[') {
                    int arrow = my_getch();
                    switch (arrow) {
                    case 'A': if (state.cursor_row > 0) state.cursor_row--; break;
                    case 'B': if (state.cursor_row < 8) state.cursor_row++; break;
                    case 'D': if (state.cursor_col > 0) state.cursor_col--; break;
                    case 'C': if (state.cursor_col < 8) state.cursor_col++; break;
                    }
                }
                break;
            }
#endif
            /* —— 数字键 1-9 —— */
            case '1': case '2': case '3':
            case '4': case '5': case '6':
            case '7': case '8': case '9':
                place_number(state, c - '0');
                break;

            /* —— 擦除键 —— */
            case '0':
            case 8:   /* Backspace */
            case 127: /* Delete (部分终端) */
                erase_cell(state);
                break;

            /* —— 重新开始 —— */
            case 'r': case 'R':
                init_game(state, difficulty);
                break;

            /* —— 退出 —— */
            case 'q': case 'Q':
                state.game_over = true;
                break;
            }

            /* 刷新画面 */
            render(state);
        }

        sleep_ms(30);
    }

    /* ===== 恢复终端 ===== */
    printf("\033[?25h"); /* 恢复光标 */

    /* 判断是胜利还是退出 */
    if (state.empty_count == 0) {
        /* 验证是否真的赢了 */
        bool won = true;
        for (int r = 0; r < BOARD_SIZE && won; r++)
            for (int c = 0; c < BOARD_SIZE && won; c++)
                if (state.board[r][c].is_conflict || state.board[r][c].value == 0)
                    won = false;

        if (won) {
            printf("\n  ╔══════════════════════════════════╗\n");
            printf("  ║  恭喜你完成了数独！              ║\n");
            printf("  ║  难度: %-25s ║\n", DIFFICULTIES[difficulty].name);
            printf("  ╚══════════════════════════════════╝\n");
        }
    }

    printf("\n  按任意键退出...\n");
    my_getch();

    console_restore();
    return 0;
}
