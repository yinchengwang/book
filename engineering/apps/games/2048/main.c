#include "2048.h"

int main(void) {
    set_utf8();
    srand((unsigned)time(NULL));

    /* ===== 标题与难度选择 ===== */
    printf("\n");
    printf("  ╔══════════════════════════════════╗\n");
    printf("  ║          2048 游戏               ║\n");
    printf("  ╠══════════════════════════════════╣\n");
    printf("  ║  1. 入门 (初始 1 个块)           ║\n");
    printf("  ║  2. 简单 (初始 2 个块)           ║\n");
    printf("  ║  3. 困难 (初始 3 个块)           ║\n");
    printf("  ║  按 Q 退出                       ║\n");
    printf("  ╚══════════════════════════════════╝\n");
    printf("  请选择难度 [1-3]: ");

    int ch = my_getch();
    printf("%c\n\n", ch);

    if (ch == 'q' || ch == 'Q') {
        printf("  再见！\n");
        return 0;
    }

    int initial_tiles;
    if      (ch == '1') initial_tiles = SINGLE_INIT_TILES;
    else if (ch == '2') initial_tiles = EASY_INIT_TILES;
    else                initial_tiles = HARD_INIT_TILES;

    /* ===== 进入原始终端模式 ===== */
    console_raw_mode();

    struct Game2048 game;
    init_game(&game, initial_tiles);
    game.best_score = 0;

    /* 隐藏光标 */
    printf("\033[?25l");

    /* 首次渲染 */
    render(&game);

    /* ===== 主循环 ===== */
    while (!game.game_over) {
        if (my_kbhit()) {
            int c = my_getch();

            /* 仅当非方向键前缀时排空缓冲区（避免吃掉方向键第二字节） */
#ifdef _WIN32
            if (c != 0xE0 && c != 0x00)
#else
            if (c != '\033')
#endif
            {
                while (my_kbhit()) my_getch();
            }

            switch (c) {
            /* —— Windows 方向键（双字节码 0xE0 前缀） —— */
#ifdef _WIN32
            case 0xE0:
            case 0x00: {
                int arrow = my_getch();
                switch (arrow) {
                case 72: move_up(&game);    break; /* 上 */
                case 80: move_down(&game);  break; /* 下 */
                case 75: move_left(&game);  break; /* 左 */
                case 77: move_right(&game); break; /* 右 */
                }
                break;
            }
#else
            case '\033': {
                int b = my_getch();
                if (b == '[') {
                    int arrow = my_getch();
                    switch (arrow) {
                    case 'A': move_up(&game);    break; /* 上 */
                    case 'B': move_down(&game);  break; /* 下 */
                    case 'D': move_left(&game);  break; /* 左 */
                    case 'C': move_right(&game); break; /* 右 */
                    }
                }
                break;
            }
#endif
            /* —— WASD 键 —— */
            case 'w': case 'W': move_up(&game);    break;
            case 's': case 'S': move_down(&game);  break;
            case 'a': case 'A': move_left(&game);  break;
            case 'd': case 'D': move_right(&game); break;

            /* —— 退出 —— */
            case 'q': case 'Q':
                game.game_over = true;
                continue;

            /* —— 重新开始 —— */
            case 'r': case 'R':
                init_game(&game, initial_tiles);
                render(&game);
                continue;
            default:
                continue; /* 忽略其他键 */
            }

            /* 发生了有效移动 */
            if (game.moved) {
                spawn_tile(&game);
                if (game.score > game.best_score)
                    game.best_score = game.score;
            }

            /* 检查胜利 */
            if (!game.won && !game.keep_going) {
                for (int r = 0; r < BOARD_SIZE; r++)
                    for (int c = 0; c < BOARD_SIZE; c++)
                        if (game.board[r][c] >= WIN_VALUE)
                            game.won = true;
            }

            /* 检查失败 */
            if (!can_move(&game))
                game.game_over = true;

            /* 更新渲染 */
            render(&game);
        }

        sleep_ms(20);
    }

    /* ===== 结束画面 ===== */
    if (game.won && !game.keep_going)
        render_win_overlay();

    if (!can_move(&game))
        render_lose_overlay();

    printf("\n  最终得分: %d\n", game.score);
    printf("  按任意键退出...\n");

    printf("\033[?25h"); /* 恢复光标 */
    my_getch();

    console_restore();
    return 0;
}
