/**
 * @file test_common_terminal.cpp
 * @brief 终端适配层测试
 */

#include <gtest/gtest.h>
#include "terminal.h"
#include <cstdlib>
#include <cstring>

// ============================================================
// 测试夹具
// ============================================================

class TerminalTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前初始化终端
        terminal_init();
    }

    void TearDown() override {
        // 每个测试后恢复终端
        terminal_restore();
    }
};

// ============================================================
// 基础功能测试
// ============================================================

TEST_F(TerminalTest, InitAndRestore) {
    // 终端初始化应该成功
    TermResult init_result = terminal_init();
    EXPECT_EQ(TERM_OK, init_result);

    // 终端恢复应该成功
    TermResult restore_result = terminal_restore();
    EXPECT_EQ(TERM_OK, restore_result);
}

TEST_F(TerminalTest, SetUtf8) {
    // 设置 UTF-8 编码应该不崩溃
    terminal_set_utf8();
    // 无返回值检查，仅验证不崩溃
}

TEST_F(TerminalTest, Clear) {
    // 清屏操作应该不崩溃
    terminal_clear();
}

TEST_F(TerminalTest, CursorVisibility) {
    // 隐藏光标应该不崩溃
    terminal_hide_cursor();

    // 显示光标应该不崩溃
    terminal_show_cursor();
}

TEST_F(TerminalTest, MoveCursor) {
    // 移动光标到指定位置应该不崩溃
    terminal_move_cursor(1, 1);
    terminal_move_cursor(10, 20);
    terminal_move_cursor(50, 80);
}

TEST_F(TerminalTest, Sleep) {
    // 延时应该正常工作
    terminal_sleep_ms(10);
    terminal_sleep_ms(100);
    terminal_sleep_ms(0);  // 零延时也应该处理
}

// ============================================================
// 按键输入测试
// ============================================================

TEST_F(TerminalTest, KbHit) {
    // kbhit 在无按键时应该返回 0
    // 注意：在某些终端上可能有延迟
    terminal_init();
    for (int i = 0; i < 3; i++) {
        int result = terminal_kbhit();
        EXPECT_EQ(0, result);
        terminal_sleep_ms(50);
    }
}

TEST_F(TerminalTest, GetchNoBlock) {
    // 在无按键时 getch 应该返回 -1
    terminal_init();
    int ch = terminal_getch();
    EXPECT_EQ(-1, ch);
}

TEST_F(TerminalTest, GetchWithTimeout) {
    // 测试带超时的 getch
    terminal_init();

    // 快速检查，无按键时立即返回
    auto start = std::chrono::steady_clock::now();
    int ch = terminal_getch();
    auto end = std::chrono::steady_clock::now();

    EXPECT_EQ(-1, ch);

    // 验证是立即返回（无阻塞超过 1 秒）
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 1000);
}

// ============================================================
// 边界测试
// ============================================================

TEST_F(TerminalTest, MoveCursorBoundary) {
    // 测试边界情况
    terminal_init();

    // 移动到角落
    terminal_move_cursor(1, 1);
    terminal_move_cursor(1, 1);

    // 移动到大坐标（可能超出实际屏幕）
    terminal_move_cursor(1000, 1000);
}

TEST_F(TerminalTest, SleepBoundary) {
    // 测试边界延时值
    terminal_init();

    // 零延时
    terminal_sleep_ms(0);

    // 大延时（但不要太长以免影响测试时间）
    terminal_sleep_ms(5);
}

TEST_F(TerminalTest, DoubleInit) {
    // 多次初始化应该安全处理
    terminal_init();
    terminal_init();
    terminal_restore();
}

TEST_F(TerminalTest, DoubleRestore) {
    // 多次恢复应该安全处理
    terminal_init();
    terminal_restore();
    terminal_restore();
}

// ============================================================
// 集成测试
// ============================================================

TEST_F(TerminalTest, FullWorkflow) {
    // 模拟完整的终端操作流程
    terminal_init();
    terminal_set_utf8();
    terminal_clear();
    terminal_hide_cursor();
    terminal_move_cursor(10, 10);

    // 模拟一些"检查"操作
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(0, terminal_kbhit());
        EXPECT_EQ(-1, terminal_getch());
        terminal_sleep_ms(10);
    }

    terminal_show_cursor();
    terminal_move_cursor(1, 1);
    terminal_restore();
}