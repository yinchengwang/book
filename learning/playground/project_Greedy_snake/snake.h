/*
 * file name：snake.h
 * author：yinchengwang
 * version：1.0
 * date：2024-10-22
 * description：The head file of snake.c
 */

#pragma once // 编译器在编译时只包含一次该文件
#include<locale.h>
#include<stdlib.h>
#include<stdio.h>
#include<windows.h>
#include<stdbool.h>
#define WALL L'□' // 打印  墙壁
#define BODY L'●' // 打印  蛇身
#define FOOD L'※' // 打印 食物


#define KEY_PRESS(VK)  ( (GetAsyncKeyState(VK) & 0x1) ? 1 : 0 )

// 蛇最初始的长度
#define SNAKE_INIT_NODE 5

// 单个食物的分数
#define FOOD_WEIGHT 5

// 步间隔
#define STEP_TIME 200

// 蛇初始坐标
#define POS_X 24
#define POS_Y 5

// 蛇的方向
enum DIRECTION
{
	UP = 1,
	DOWN,
	LEFT,
	RIGHT
};

// 蛇的状态
enum STATE
{
	ALIVE = 1,
	KILL_BY_WALL,
	KILL_BY_SELF,
	ESC,
	END_NOMAL
};

// 蛇身节点结构体
typedef struct SnakeNode
{
	int x; // 坐标
	int y; // 坐标
	struct SnakeNode* next; // 指向下一个节点的指针
} SnakeNode;

// 管理整个贪吃蛇
typedef struct Snake
{
	SnakeNode *head; // 指向蛇头
	SnakeNode *food; // 地图上的食物点
	enum DIRECTION dir; // 蛇头的方向
	enum STATE stat; // 当前游戏进行状态
	int score; // 分数
	int food_weight; // 每个食物的分数
	int step_time; // 每一步的间隔
} Snake;


void set_pos(int x, int y);

// 游戏开始前的准备
void game_start(Snake *snake);

// 游戏运行过程
void game_run(Snake *snake);

// 游戏结束后释放资源
void game_end(Snake *snake);
