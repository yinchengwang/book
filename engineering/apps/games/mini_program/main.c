/**
 * @file main.c
 * @brief 游戏小程序统一入口
 *
 * 提供三个游戏的统一入口，通过菜单选择后启动独立游戏程序。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/terminal.h"
#include "../../common/menu.h"

/* 游戏启动函数 */
static void launch_snake(void) {
    terminal_restore();
    printf("\n正在启动贪吃蛇...\n");
    system("\"C:\\code\\book\\apps\\games\\snake\\snake_game.exe\"");
}

static void launch_sudoku(void) {
    terminal_restore();
    printf("\n正在启动数独...\n");
    system("\"C:\\code\\book\\apps\\games\\sudoku\\sudoku.exe\"");
}

static void launch_2048(void) {
    terminal_restore();
    printf("\n正在启动 2048...\n");
    system("\"C:\\code\\book\\apps\\games\\2048\\game2048.exe\"");
}

/* 游戏列表 */
static const GameEntry GAMES[] = {
    { "1. 贪吃蛇", launch_snake },
    { "2. 数独",   launch_sudoku },
    { "3. 2048",   launch_2048 },
};

int main(void) {
    terminal_set_utf8();

    while (1) {
        int choice = menu_main_select(GAMES, 3);
        if (choice < 0) break;

        /* 启动选中的游戏 */
        GAMES[choice].launch();

        /* 游戏结束后重新初始化终端 */
        terminal_set_utf8();
        printf("\n游戏结束，返回主菜单...\n\n");
    }

    return 0;
}
