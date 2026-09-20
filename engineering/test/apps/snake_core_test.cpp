#include <gtest/gtest.h>

extern "C" {
#include <snake_core.h>
}

namespace {

/** 使用固定 seed 创建游戏（用于确定性测试） */
SnakeGame create_fixed(int seed, int difficulty) {
    SnakeGame g;
    snake_create(&g, seed, difficulty);
    return g;
}

}  // namespace

// 1. CreateInitializesSnake — snake_create 后 len >= 3，未 game_over，score = 0
TEST(SnakeCoreTest, CreateInitializesSnake) {
    SnakeGame g;
    snake_create(&g, 42, 0);

    EXPECT_GE(g.len, 3);
    EXPECT_FALSE(g.game_over);
    EXPECT_EQ(g.score, 0);
}

// 2. MoveUpDecreasesY — 向上移动后蛇头 y 坐标减 1
TEST(SnakeCoreTest, MoveUpDecreasesY) {
    SnakeGame g = create_fixed(42, 0);
    int old_y = g.body[0].y;

    snake_input(&g, SNAKE_UP);
    snake_tick(&g);

    EXPECT_EQ(g.body[0].y, old_y - 1);
}

// 3. ReverseDirectionRejected — 反向输入不改变方向（如向右时输入左）
TEST(SnakeCoreTest, ReverseDirectionRejected) {
    SnakeGame g = create_fixed(42, 0);
    g.dir = SNAKE_RIGHT;
    g.next_dir = SNAKE_RIGHT;

    snake_input(&g, SNAKE_LEFT);  // 反向输入

    // 方向不应改变
    EXPECT_EQ(g.dir, SNAKE_RIGHT);
    EXPECT_EQ(g.next_dir, SNAKE_RIGHT);
}

// 4. EatFoodGrows — 手动设置蛇头前方有食物，tick 后 len+1, score+5
TEST(SnakeCoreTest, EatFoodGrows) {
    SnakeGame g = create_fixed(42, 0);

    // 记住吃食物前的状态
    int old_len = g.len;
    int old_score = g.score;

    // 手动将食物放在蛇头前方一格
    SnakePoint head = g.body[0];
    if (g.dir == SNAKE_RIGHT) {
        g.food.x = head.x + 1;
        g.food.y = head.y;
    } else if (g.dir == SNAKE_LEFT) {
        g.food.x = head.x - 1;
        g.food.y = head.y;
    } else if (g.dir == SNAKE_UP) {
        g.food.x = head.x;
        g.food.y = head.y - 1;
    } else {
        g.food.x = head.x;
        g.food.y = head.y + 1;
    }

    snake_tick(&g);

    EXPECT_EQ(g.len, old_len + 1);
    EXPECT_EQ(g.score, old_score + 5);
}

// 5. WallCollisionGameOver — 蛇头在边界时 tick，game_over = true
TEST(SnakeCoreTest, WallCollisionGameOver) {
    SnakeGame g = create_fixed(42, 0);

    // 将蛇头移到左边界，并设置方向向左，这样 tick 后会撞墙
    g.body[0].x = 1;
    g.body[0].y = SNAKE_HEIGHT / 2;
    g.dir = SNAKE_LEFT;
    g.next_dir = SNAKE_LEFT;

    snake_tick(&g);

    EXPECT_TRUE(g.game_over);
}

// 6. SelfCollisionGameOver — 蛇头撞到自己身体时 tick，game_over = true
TEST(SnakeCoreTest, SelfCollisionGameOver) {
    SnakeGame g = create_fixed(42, 0);

    // 将蛇头位置直接设置到 body[1] 的位置，使其在 tick 时撞上自己
    // 先向右移动一格，然后向左移动使其撞上身体
    snake_tick(&g);  // 正常移动一次，蛇身变为 {{?,?}, {body[0]}, {body[1]}}
    // 现在将蛇头设为 body[1] 的位置，并设置方向向左
    g.body[0] = g.body[1];  // 蛇头和 body[1] 重叠
    g.dir = SNAKE_LEFT;
    g.next_dir = SNAKE_LEFT;

    snake_tick(&g);

    EXPECT_TRUE(g.game_over);
}

// 7. FoodNotOnSnake — snake_create 后验证食物不在蛇身上
TEST(SnakeCoreTest, FoodNotOnSnake) {
    SnakeGame g;
    snake_create(&g, 42, 0);

    bool food_on_snake = false;
    for (int i = 0; i < g.len; i++) {
        if (g.body[i].x == g.food.x && g.body[i].y == g.food.y) {
            food_on_snake = true;
            break;
        }
    }

    EXPECT_FALSE(food_on_snake);
}
