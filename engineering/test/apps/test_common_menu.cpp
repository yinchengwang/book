/**
 * @file test_common_menu.cpp
 * @brief 菜单系统测试
 */

#include <gtest/gtest.h>
#include "menu.h"
#include "terminal.h"
#include <cstring>

// ============================================================
// 测试夹具
// ============================================================

class MenuTest : public ::testing::Test {
protected:
    void SetUp() override {
        terminal_init();
    }

    void TearDown() override {
        terminal_restore();
    }
};

// ============================================================
// 辅助函数
// ============================================================

// 模拟游戏启动函数
static bool g_snake_launched = false;
static bool g_sudoku_launched = false;
static bool g_2048_launched = false;

extern "C" void mock_launch_snake(void) {
    g_snake_launched = true;
}

extern "C" void mock_launch_sudoku(void) {
    g_sudoku_launched = true;
}

extern "C" void mock_launch_2048(void) {
    g_2048_launched = true;
}

// 重置模拟状态
static void reset_mock_state() {
    g_snake_launched = false;
    g_sudoku_launched = false;
    g_2048_launched = false;
}

// ============================================================
// 菜单显示函数测试
// ============================================================

TEST_F(MenuTest, ShowBanner) {
    // 显示横幅应该不崩溃
    menu_show_banner("Test Game");
    menu_show_banner("");
    menu_show_banner("A Very Long Game Title That Might Be Longer Than Terminal Width");
}

TEST_F(MenuTest, ShowLine) {
    // 显示分隔线应该不崩溃
    menu_show_line('-', 40);
    menu_show_line('=', 80);
    menu_show_line('*', 10);
    menu_show_line('.', 0);
}

TEST_F(MenuTest, ShowLineBoundary) {
    // 超长宽度
    menu_show_line('-', 500);

    // 零宽度
    menu_show_line('=', 0);
}

// ============================================================
// 游戏入口测试
// ============================================================

TEST_F(MenuTest, GameEntryStructure) {
    // 测试 GameEntry 结构
    GameEntry games[3] = {
        {"1. 贪吃蛇", mock_launch_snake},
        {"2. 数独", mock_launch_sudoku},
        {"3. 2048", mock_launch_2048}
    };

    EXPECT_STREQ("1. 贪吃蛇", games[0].name);
    EXPECT_STREQ("2. 数独", games[1].name);
    EXPECT_STREQ("3. 2048", games[2].name);

    EXPECT_NE(nullptr, games[0].launch);
    EXPECT_NE(nullptr, games[1].launch);
    EXPECT_NE(nullptr, games[2].launch);
}

TEST_F(MenuTest, LaunchGameEntries) {
    // 测试游戏启动函数指针
    GameEntry games[3] = {
        {"1. 贪吃蛇", mock_launch_snake},
        {"2. 数独", mock_launch_sudoku},
        {"3. 2048", mock_launch_2048}
    };

    reset_mock_state();

    // 调用各个启动函数
    games[0].launch();
    EXPECT_TRUE(g_snake_launched);

    games[1].launch();
    EXPECT_TRUE(g_sudoku_launched);

    games[2].launch();
    EXPECT_TRUE(g_2048_launched);
}

// ============================================================
// 边界测试
// ============================================================

TEST_F(MenuTest, NullFunctionPointer) {
    // 空函数指针也应该安全处理
    GameEntry game_with_null[] = {
        {"1. 空游戏", nullptr}
    };

    if (game_with_null[0].launch != nullptr) {
        game_with_null[0].launch();
    }
}

TEST_F(MenuTest, LongGameName) {
    // 超长游戏名称
    const char *long_name = "这是一个非常非常非常非常非常非常非常"
                            "非常非常非常非常非常非常长的游戏名称";
    GameEntry game[] = {
        {long_name, mock_launch_snake}
    };

    EXPECT_GT(strlen(game[0].name), 100);
}

TEST_F(MenuTest, UnicodeGameName) {
    // Unicode 游戏名称
    GameEntry game[] = {
        {"🎮 游戏中心", mock_launch_snake},
        {"俄罗斯方块 Тетрис", mock_launch_sudoku},
        {"スネーク 贪吃蛇", mock_launch_2048}
    };

    EXPECT_NE(nullptr, game[0].name);
    EXPECT_NE(nullptr, game[1].name);
    EXPECT_NE(nullptr, game[2].name);
}