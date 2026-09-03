#include "snake.h"

int main(void) {
    set_utf8();
    srand((unsigned)time(NULL));

    printf("\n========== 贪吃蛇 ==========\n");
    printf("1. 简单 (速度 180ms)\n");
    printf("2. 困难 (速度 120ms)\n");
    printf("3. 地狱 (速度 80ms)\n");
    printf("选择难度 [1-3]: ");

    int ch = my_getch();
    printf("%c\n", ch);

    if (ch == '1')      { diff = DIFF_EASY; base_speed = 180; }
    else if (ch == '2') { diff = DIFF_HARD; base_speed = 120; }
    else if (ch == '3') { diff = DIFF_HELL; base_speed = 80; }
    else                { diff = DIFF_EASY; base_speed = 180; }

    printf("按 WASD 或方向键移动，回车开始...\n");
    my_getch();

    console_raw_mode();
    init_game();

    printf("\033[?25l");  /* 隐藏光标 */

    int speed_boost = 0;

    while (!game_over) {
        if (my_kbhit()) {
            int prev_next_dir = next_dir;
            int had_dir_input = 0;

            /* 排空输入缓冲区，只保留最后一次有效方向 */
            while (my_kbhit()) {
                int c = my_getch();
#ifdef _WIN32
                if (c == 0 || c == 224) {
                    int arrow = my_getch();
                    switch (arrow) {
                        case 72: next_dir = DIR_UP;    had_dir_input = 1; break;
                        case 80: next_dir = DIR_DOWN;  had_dir_input = 1; break;
                        case 75: next_dir = DIR_LEFT;  had_dir_input = 1; break;
                        case 77: next_dir = DIR_RIGHT; had_dir_input = 1; break;
                    }
                } else
#endif
                {
                    switch (c) {
                        case 'w': case 'W': next_dir = DIR_UP;    had_dir_input = 1; break;
                        case 's': case 'S': next_dir = DIR_DOWN;  had_dir_input = 1; break;
                        case 'a': case 'A': next_dir = DIR_LEFT;  had_dir_input = 1; break;
                        case 'd': case 'D': next_dir = DIR_RIGHT; had_dir_input = 1; break;
                        case 'q': case 'Q': game_over = 1; break;
                    }
                }
            }

            /* 方向没变且有效 → 按住加速 */
            speed_boost = had_dir_input && (next_dir == prev_next_dir);
        } else {
            speed_boost = 0;
        }

        update();
        render();
        sleep_ms(speed_boost ? speed / 2 : speed);
    }

    printf("\033[?25h");  /* 恢复光标 */

    printf("\n游戏结束！得分: %d\n", score);
    printf("按任意键退出...\n");
    my_getch();

    console_restore();
    return 0;
}
