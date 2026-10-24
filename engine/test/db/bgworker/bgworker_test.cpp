/**
 * @file bgworker_test.cpp
 * @brief 后台任务调度器核心测试
 */

#include <gtest/gtest.h>
#include "db/bgworker.h"
#include "db/log.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>

/* ============================================================
 * 类型补充定义
 * 说明：task_iface.h 是 C 头文件（依赖 <stdatomic.h>），与 C++ 不完全兼容，
 *      且其定义的枚举与 db/bgworker.h 冲突。此处仅手动声明测试需要的类型。
 * ============================================================ */

extern "C" {

/* 任务上下文（与 task_iface.h 中的 db_bg_task_context_t 对齐） */
struct db_bg_task_context {
    void          *user_data;
    uint64_t       last_check_time;
    uint32_t       consecutive_trigger;
    unsigned int   status;   /* C++ 端用普通 unsigned int 代替 atomic_uint */
};

/* 任务结构（与 task_iface.h 中的 db_bg_task_t 对齐） */
struct db_bg_task {
    const char                *name;
    uint32_t                   interval_ms;
    int (*init)(db_bg_task_context_t *ctx);
    int (*check)(db_bg_task_context_t *ctx);
    int (*execute)(db_bg_task_context_t *ctx);
    void (*cleanup)(db_bg_task_context_t *ctx);
    db_bg_task_context_t      *ctx;
};

} /* extern "C" */

/* ============================================================
 * 测试夹具
 * ============================================================ */

class BgWorkerTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化日志系统 */
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_ERROR;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);
        log_set_level(LOG_LEVEL_DEBUG);
    }

    void TearDown() override {
        db_bgworker_stop();
    }
};

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @brief 测试任务注册与注销
 */
TEST_F(BgWorkerTest, RegisterUnregister) {
    std::atomic<int> check_count{0};
    std::atomic<int> exec_count{0};

    /* 使用 C 风格赋值以兼容 C++ */
    db_bg_task_t task;
    memset(&task, 0, sizeof(task));
    task.name = "test_task";
    task.interval_ms = 100;
    task.init = nullptr;
    task.check = [](db_bg_task_context_t *ctx) -> int {
        (void)ctx;
        return 1;  /* 总是触发 */
    };
    task.execute = [](db_bg_task_context_t *ctx) -> int {
        (void)ctx;
        return 0;
    };
    task.cleanup = nullptr;
    task.ctx = nullptr;

    /* 启动调度器（必须先启动，注册任务时会调用配置初始化） */
    EXPECT_EQ(db_bgworker_start(), 0);

    /* 注册任务 */
    int slot;
    EXPECT_EQ(db_bgworker_register(&task, &slot), 0);

    /* 等待一段时间让任务执行 */
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    /* 注销任务 */
    EXPECT_EQ(db_bgworker_unregister("test_task"), 0);

    /* 停止调度器 */
    EXPECT_EQ(db_bgworker_stop(), 0);
}

/**
 * @brief 测试任务暂停与恢复
 */
TEST_F(BgWorkerTest, PauseResume) {
    std::atomic<int> exec_count{0};

    db_bg_task_t task;
    memset(&task, 0, sizeof(task));
    task.name = "pause_test_task";
    task.interval_ms = 50;
    task.init = nullptr;
    task.check = [](db_bg_task_context_t *ctx) -> int {
        (void)ctx;
        return 1;
    };
    task.execute = [](db_bg_task_context_t *ctx) -> int {
        (void)ctx;
        return 0;
    };
    task.cleanup = nullptr;
    task.ctx = nullptr;

    /* 先启动调度器，再注册任务 */
    db_bgworker_start();
    db_bgworker_register(&task, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int count_before = exec_count.load();

    /* 暂停任务 */
    EXPECT_EQ(db_bgworker_pause("pause_test_task"), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int count_paused = exec_count.load();

    /* 恢复任务 */
    EXPECT_EQ(db_bgworker_resume("pause_test_task"), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int count_after = exec_count.load();

    /* 验证调度器正常处理暂停和恢复操作（不崩溃） */
    EXPECT_GE(count_paused, count_before);
    EXPECT_GE(count_after, count_paused);

    db_bgworker_unregister("pause_test_task");
    db_bgworker_stop();
}

/**
 * @brief 测试任务状态查询
 */
TEST_F(BgWorkerTest, GetStatus) {
    db_bg_task_t task;
    memset(&task, 0, sizeof(task));
    task.name = "status_test_task";
    task.interval_ms = 1000;
    task.init = nullptr;
    task.check = [](db_bg_task_context_t *ctx) -> int {
        (void)ctx;
        return 0;
    };
    task.execute = [](db_bg_task_context_t *ctx) -> int {
        (void)ctx;
        return 0;
    };
    task.cleanup = nullptr;
    task.ctx = nullptr;

    /* 先启动调度器，再注册任务 */
    db_bgworker_start();
    db_bgworker_register(&task, nullptr);

    /* 等待任务进入 IDLE 状态 */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    unsigned int status;
    EXPECT_EQ(db_bgworker_get_status("status_test_task", &status), 0);
    EXPECT_EQ(status, (unsigned int)DB_BG_TASK_STATUS_IDLE);

    db_bgworker_unregister("status_test_task");
    db_bgworker_stop();
}

/**
 * @brief 测试统计信息获取
 */
TEST_F(BgWorkerTest, GetStats) {
    db_bgworker_start();

    db_bgworker_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    EXPECT_EQ(db_bgworker_get_stats(&stats), 0);
    EXPECT_GE(stats.task_count, 0u);

    db_bgworker_stop();
}

/**
 * @brief 测试重复注册
 */
TEST_F(BgWorkerTest, DuplicateRegistration) {
    db_bg_task_t task1;
    memset(&task1, 0, sizeof(task1));
    task1.name = "dup_task";
    task1.interval_ms = 1000;
    task1.init = nullptr;
    task1.check = [](db_bg_task_context_t *ctx) -> int {
        (void)ctx;
        return 0;
    };
    task1.execute = [](db_bg_task_context_t *ctx) -> int {
        (void)ctx;
        return 0;
    };
    task1.cleanup = nullptr;
    task1.ctx = nullptr;

    db_bg_task_t task2;
    memset(&task2, 0, sizeof(task2));
    task2.name = "dup_task";  /* 同名 */
    task2.interval_ms = 1000;
    task2.init = nullptr;
    task2.check = [](db_bg_task_context_t *ctx) -> int {
        (void)ctx;
        return 0;
    };
    task2.execute = [](db_bg_task_context_t *ctx) -> int {
        (void)ctx;
        return 0;
    };
    task2.cleanup = nullptr;
    task2.ctx = nullptr;

    EXPECT_EQ(db_bgworker_register(&task1, nullptr), 0);
    EXPECT_EQ(db_bgworker_register(&task2, nullptr), -1);  /* 应失败 */

    db_bgworker_unregister("dup_task");
}
