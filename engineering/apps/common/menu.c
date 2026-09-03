/**
 * @file menu.c
 * @brief 统一游戏菜单系统实现
 */

#include "menu.h"
#include "terminal.h"
#include <stdio.h>
#include <string.h>

#define MAX_WIDTH 60

void menu_show_line(char ch, int width) {
    if (width <= 0) width = MAX_WIDTH;
    for (int i = 0; i < width; i++) {
        putchar(ch);
    }
    putchar('\n');
}

void menu_show_banner(const char *title) {
    int width = 40;
    printf("\n");
    menu_show_line('=', width);
    printf("  %s\n", title);
    menu_show_line('=', width);
}

int menu_main_select(const GameEntry *games, int count) {
    if (!games || count <= 0) return -1;

    while (1) {
        terminal_clear();
        menu_show_banner("游戏中心");

        for (int i = 0; i < count; i++) {
            printf("  %s\n", games[i].name);
        }
        printf("  %d. 退出\n", count);
        menu_show_line('-', 40);

        printf("\n  请选择游戏 [1-%d] 或 %d 退出: ", count, count);

        int ch = terminal_getch();
        printf("%c\n\n", ch);

        /* 数字键选择 */
        if (ch >= '1' && ch <= '9') {
            int choice = ch - '0';
            if (choice >= 1 && choice <= count) {
                return choice - 1;
            }
        }

        /* 退出键 */
        if (ch == '0' || ch == 'q' || ch == 'Q') {
            printf("  再见！\n");
            return -1;
        }
    }
}

int menu_difficulty_select(const char *title, const char *const *options, int count) {
    if (!title || !options || count <= 0) return -1;

    menu_show_banner(title);

    for (int i = 0; i < count; i++) {
        printf("  %d. %s\n", i + 1, options[i]);
    }
    printf("  按 Q 退出\n");
    menu_show_line('-', 40);

    printf("\n  请选择难度 [1-%d]: ", count);

    while (1) {
        int ch = terminal_getch();
        printf("%c\n", ch);

        /* 数字键选择 */
        if (ch >= '1' && ch <= '9') {
            int choice = ch - '0';
            if (choice >= 1 && choice <= count) {
                return choice - 1;
            }
        }

        /* 退出键 */
        if (ch == 'q' || ch == 'Q') {
            return -1;
        }
    }
}

void menu_wait_exit(void) {
    printf("\n  按任意键退出...\n");
    terminal_getch();
}
