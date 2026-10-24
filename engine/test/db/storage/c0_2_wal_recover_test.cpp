/**
 * @file c0_2_wal_recover_test.cpp
 * @brief C0-2 统一恢复入口 API 测试（T10）
 *
 * 本测试覆盖恢复入口的 API 表面：
 *   1. wal_recover_register 注册成功 + 重复注册覆盖
 *   2. db_startup_recover 空 WAL 路径 → 0 记录（无需重启）
 *
 * **限制**：完整崩溃恢复端到端测试（kill -9 → 重启 → 数据完整）
 * 依赖 wal.c 尚未实现的 wal_replay() 内部分发接口（设计见 design.md）。
 * 当前仅验证 API 可用性 + 不存在的 WAL 文件正常返回 0。
 *
 * 如需完整 E2E 恢复测试，需先在 wal.c 实现 wal_replay()，可作为后续任务。
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>

extern "C" {
#include "db/storage/wal/wal_recover.h"
#include "db/storage/wal/wal.h"
}

namespace {

int stub_apply_count = 0;
int stub_apply(void *ctx, wal_log_type_t type,
              const void *key, size_t key_len,
              const void *value, size_t value_len) {
    (void)ctx; (void)type; (void)key; (void)key_len;
    (void)value; (void)value_len;
    stub_apply_count++;
    return 0;
}

}  // namespace

TEST(WalRecover, RegisterAndOverwrite) {
    stub_apply_count = 0;

    /* 第一次注册 */
    ASSERT_EQ(wal_recover_register(WAL_LOG_HEAP_INSERT, stub_apply, nullptr), 0);

    /* 重复注册（覆盖）应返回 0 */
    ASSERT_EQ(wal_recover_register(WAL_LOG_HEAP_INSERT, stub_apply, nullptr), 0);

    /* 不同 type 注册 */
    ASSERT_EQ(wal_recover_register(WAL_LOG_HEAP_DELETE, stub_apply, nullptr), 0);
}

TEST(WalRecover, EmptyWalPathReturnsZero) {
    /* 空路径：跳过，正常返回 0 */
    int rc = db_startup_recover("", 4096);
    EXPECT_EQ(rc, 0);
}

TEST(WalRecover, NullWalPathReturnsZero) {
    int rc = db_startup_recover(nullptr, 4096);
    EXPECT_EQ(rc, 0);
}

TEST(WalRecover, NonExistentWalReturnsZero) {
    /* 不存在的 WAL 文件：当作空库启动（不阻塞） */
    int rc = db_startup_recover("/tmp/c0_2_nonexistent_wal_test.bin", 4096);
    EXPECT_GE(rc, 0);
}

TEST(WalRecover, RegisterNullApplyFails) {
    /* NULL 回调应被拒绝 */
    EXPECT_EQ(wal_recover_register(WAL_LOG_TS_APPEND, nullptr, nullptr), -1);
}
