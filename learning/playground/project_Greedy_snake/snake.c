/*
 * file name：snake.c
 * author：yinchengwang
 * version：1.0
 * date：2024-10-22
 * description：The main code of the greedy snake
 */

#include "snake.h"

// 设置打印信息在屏幕中的位置
void set_pos(int x, int y)
{
	//获取标准输出设备的句柄
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	//设置标准输出上光标的位置为pos
	COORD pos = { x, y };
	SetConsoleCursorPosition(handle, pos);
}

// 打印欢迎信息，只要目的是为了打印的界面好看
void welcome_to_game()
{
	set_pos(40, 13);
	printf("欢迎来到贪吃蛇小游戏\n");

	set_pos(36, 16); // Press any key to continue . . .的位置
 
	system("pause");

	system("cls"); // 清空屏幕准备打印下一波信息
 
	//二、功能介绍页面
	set_pos(36, 8);
	printf("1、用  ↑   ↓   ←    →  来控制蛇的移动  \n");
	set_pos(36, 10);
	printf("2、F3 是 加速, F4 是 减速     O(∩_∩)O\n");
	set_pos(36, 12);
	printf("3、加速可以获得更高的分数    (づ￣ 3￣)づ\n");
	set_pos(36, 16); // Press any key to continue . . .的位置
	system("pause");

	system("cls");
}

 
void create_map()
{
	// 上边界 每隔两格打印一个 宽字符
	for (int i = 0; i <= 56; i += 2)
	{
		set_pos(i, 0);
		wprintf(L"%c", WALL);
	}

	// 下边界
	for (int i = 0; i <= 56; i += 2)
	{
		set_pos(i, 26);
		wprintf(L"%c", WALL);
	}

	// 左边界
	for (int i = 1; i <= 25; i++)
	{
		set_pos(0, i);
		wprintf(L"%c", WALL);
	}

	// 右边界
	for (int i = 1; i <= 25; i++)
	{
		set_pos(56, i);
		wprintf(L"%c", WALL);
	}

	return;
}

void snake_init(Snake *snake)
{
	// 创建5个节点: 
	SnakeNode *cur = NULL;
	for (int i = 0; i < SNAKE_INIT_NODE; i++)
	{
		cur = (SnakeNode *)malloc(sizeof(SnakeNode));
		if (cur == NULL)
		{
			perror("InitSnake:malloc");
			return;
		}
		cur->x = POS_X + 2 * i;
		cur->y = POS_Y;
		cur->next = NULL;
 
		// 食物从头部开始吃, 使用头插法比较合适
		if (snake->head == NULL) {
			snake->head = cur;
		} else {
			cur->next = snake->head;
			snake->head = cur;
		}
	}

	// 初始化蛇身
	cur = snake->head;
	while (cur) {
		set_pos(cur->x, cur->y);
		wprintf(L"%lc", BODY);
		cur = cur->next;
	}
 
	// 初始化贪吃蛇的其他信息
	snake->dir = RIGHT;
	snake->food_weight = FOOD_WEIGHT;
	snake->food = NULL;
	snake->score = 0;
	snake->step_time = STEP_TIME;
	snake->stat = ALIVE;
}

/* 创建食物
思路：
	1、食物在规定的地图范围内出现即可
	2、x轴的范围 2 - 54 ---> rand() % 53  + 2  x占 2 个窄字符的,x的取值必须是偶数,不然不能全覆盖
	3、y轴的范围 1 - 25 --> rand() % 25 + 1
*/
void create_food(Snake *snake)
{
	// 1.食物是随机出现的，坐标就是随机的
	// 2.坐标必须在墙内
	int x = 0;
	int y = 0;

again:
	do {
		x = rand() % 53 + 2;
		y = rand() % 25 + 1;
	} while (x % 2 != 0);
 
	// 3.坐标不能在初始化时的蛇身体上
	SnakeNode *cur = snake->head;
	while (cur) {
		if (x == cur->x && y == cur->y) {
			goto again;
		} else {
			cur = cur->next;
		}
	}
 
	//创建食物: 一个新节点
	SnakeNode *food = (SnakeNode *)malloc(sizeof(SnakeNode));
	if (food == NULL) {
		perror("create_food():malloc()");
		return;
	}
	food->x = x;
	food->y = y;
	snake->food = food; // 新食物节点, 用于移动时判定是否吃到了食物
 
	// 打印食物
	set_pos(x, y);
	wprintf(L"%c", FOOD);
}

void kill_by_self(Snake *snake)
{
	SnakeNode *curr = snake->head->next;
	while (curr != NULL) {
		if (curr->x == snake->head->x && curr->y == snake->head->y) {
			snake->stat = KILL_BY_SELF;
			return;
		}
		curr = curr->next;
	}
}

void kill_by_wall(Snake *snake)
{
	if (snake->head->x == 0 || snake->head->x == 56 ||
		snake->head->y == 0 || snake->head->y == 26) {
		snake->stat = KILL_BY_WALL;
	}
}

void not_eat_food(Snake *snake, SnakeNode *pNext)
{
	// 头插法：链接方向指针
	pNext->next = snake->head;
	snake->head = pNext;
 
	// 单链表的尾删法
	SnakeNode *cur = snake->head;
	while (cur->next->next)
	{
		set_pos(cur->x, cur->y);
		wprintf(L"%lc", BODY);
		cur = cur->next;
	}

	set_pos(cur->next->x, cur->next->y);
	printf("  ");

	free(cur->next);
	cur->next = NULL;
}

void eat_food(Snake *snake, SnakeNode *pNext)
{
	pNext->next = snake->head;
	snake->head = pNext;

	SnakeNode *cur = snake->head;
	while (cur) {
		set_pos(cur->x, cur->y);
		wprintf(L"%c", BODY);
		cur = cur->next;
	}

	// 分数变化：
	snake->score += snake->food_weight;

	// 释放食物节点
	free(snake->food);

	//创建新食物;
	create_food(snake);
}

bool next_is_food(Snake *snake, SnakeNode *pNext)
{
	if (snake->food->x == pNext->x && snake->food->y == pNext->y) {
		return true;
	}

	return false;
}


// 移动的本质：没吃食物, 则长度不变, 将方向节点链接到蛇头上, free掉尾节点
void snake_move(Snake *snake)
{
	// 创建下一个节点
	SnakeNode *pNext = (SnakeNode *)malloc(sizeof(SnakeNode));
	if (pNext == NULL) {
		perror("SnakeMove():malloc:");
		return;
	}
	pNext->next = NULL;

	// 根据head的坐标更新下一个节点的方向
	switch (snake->dir) {
		case UP:
			pNext->x = snake->head->x;
			pNext->y = snake->head->y - 1;
			break;
		case DOWN:
			pNext->x = snake->head->x;
			pNext->y = snake->head->y + 1;
			break;
		case LEFT:
			pNext->x = snake->head->x - 2;
			pNext->y = snake->head->y;
			break;
		case RIGHT:
			pNext->x = snake->head->x + 2;
			pNext->y = snake->head->y;
			break;
	}

	//下一个坐标处是否是食物
	if (next_is_food(snake, pNext)) {
		eat_food(snake, pNext);
	} else {
		not_eat_food(snake, pNext);
	}
 
	kill_by_wall(snake);
	kill_by_self(snake);
}

//游戏暂停
void pause_in_game()
{
	while (1) {
		Sleep(200);
		if (KEY_PRESS(VK_SPACE)) {
			break;
		}
	}
}

void game_start(Snake *snake)
{
	// 一、设置窗口大小
    system("mode con cols=100 lines=30"); // 设置窗口大小
	system("title greedy snake"); // 设置cmd窗口名称, (中文标题还是乱码)

	// 二、隐藏光标：不想光标在一直闪
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO cursorinfo;
	GetConsoleCursorInfo(handle, &cursorinfo);
	cursorinfo.bVisible = false;  //隐藏控制台光标
	SetConsoleCursorInfo(handle, &cursorinfo); //设置控制台光标状态

	// 三、打印游戏欢迎信息
    welcome_to_game();

	// 四、绘制地图
	create_map();

	// 五、初始化蛇
	snake_init(snake);

	// 六、初始化食物
	create_food(snake);
}

void print_help_info(Snake *snake)
{
	// 设置位置
	set_pos(65, 2);
	printf("温馨提示\n");
	set_pos(65, 4);
	printf("1、不能穿墙\n");
	set_pos(65, 6);
	printf("2、不能咬到自己\n");
	set_pos(65, 8);
	printf("3、按   ESC   退出游戏\n");
	set_pos(65, 10);
	printf("4、按   空格  暂停游戏\n");
	set_pos(65, 12);
	printf("操作回顾\n");
	set_pos(65, 14);
	printf("5、用 ↑↓ ← → 来控制蛇的移动  \n");
	set_pos(65, 16);
	printf("6、F3 加速, F4 减速\n");
	set_pos(65, 18);
	printf("7、加速可以获得更高的分数\n");

	set_pos(65, 20);
	printf("当前速度: %d\n", snake->step_time);
	set_pos(65, 22);
	printf("单个食物分数: %d\n", snake->food_weight);
	set_pos(65, 24);
	printf("当前分数: %d\n", snake->score);
}


// 游戏运行过程
void game_run(Snake *snake)
{
	do {
		print_help_info(snake);
		// 检测按键: 判断发出的指令
		// 上下左右, ESC, 空格, F3, F4
		if (KEY_PRESS(VK_UP) && snake->dir != DOWN) {
			snake->dir = UP;
		} else if (KEY_PRESS(VK_DOWN) && snake->dir != UP) {
			snake->dir = DOWN;
		} else if (KEY_PRESS(VK_LEFT) && snake->dir != RIGHT) {
			snake->dir = LEFT;
		} else if (KEY_PRESS(VK_RIGHT) && snake->dir != LEFT) {
			snake->dir = RIGHT;
		} else if (KEY_PRESS(VK_ESCAPE)) {
			snake->stat = ESC; // 将状态该成退出状态
			break;
		} else if (KEY_PRESS(VK_SPACE)) {
			pause_in_game(); //暂定和回复暂定
		} else if (KEY_PRESS(VK_F3)) { // 加速: 即休眠时间变短, 同时分数变多, 而且次数需要有限制
			if (snake->step_time >= 80) { // 最低下限：step_time = 80 (初始化sleep 是 200ms)
				snake->step_time -= 30;
				snake->food_weight += 2;
			}
		} else if (KEY_PRESS(VK_F4)) { // 减速: 即休眠时间变长, 同时分数变少
			if (snake->food_weight > 2) { // 最低下限：food_weight > 2
				snake->step_time += 30;
				snake->food_weight -= 2;
			}
		}

		snake_move(snake);

		Sleep(snake->step_time);
	} while (snake->stat == ALIVE);

	return;
}

// 游戏结束后释放资源
void game_end(Snake *snake)
{
	set_pos(15, 12);
	switch (snake->stat) {
		case ESC:
			printf("主动退出游戏\n");
			break;
		case KILL_BY_WALL:
			printf("很遗憾，撞墙了，游戏结束\n");
			break;
		case KILL_BY_SELF:
			printf("很遗憾，撞到自己，游戏结束\n");
			break;
	}
 
	//释放
	SnakeNode *cur = snake->head;
	SnakeNode *del = NULL;
 
	while (cur) {
		del = cur;
		cur = cur->next;
		free(del);
		del = NULL;
	}

	free(snake->food);
	snake->food = NULL;
	snake->head = NULL;
}
