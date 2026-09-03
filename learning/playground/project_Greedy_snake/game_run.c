/*
 * file name：game_run.c
 * author：yinchengwang
 * version：1.0
 * date：2024-10-22
 * description：to run the greedy snake game
 */

#include "snake.h"
#include<locale.h> // 包含头文件

int main()
{
    //切换到本地环境：中文环境
    setlocale(LC_ALL, "");

    int ch = 0;

    Snake snake = { 0 };
    do {
        game_start(&snake); // 初始化, 按键提示, 墙体生成, 初始化蛇, 生成第一个食物等等
        game_run(&snake); // 游戏过程
        game_end(&snake); // 游戏结束
        set_pos(POS_X, POS_Y);
        printf("怎么说, 再冲一把否? (y/n)");
		ch = getchar(); // y/n
		getchar(); // enter
    } while (ch == 'Y' || ch == 'y');

    return 0;
}