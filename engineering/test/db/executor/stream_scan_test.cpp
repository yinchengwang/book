/**
 * @file stream_scan_test.cpp
 * @brief 流处理算子单元测试
 *
 * 测试流引擎及 4 个 Volcano 流算子：
 * - StreamScan（流扫描）
 * - StreamWindow（流窗口）
 * - StreamJoin（流连接）
 * - StreamAgg（流聚合）
 */
#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "db/executor/stream/stream_scan.h"
#include "db/executor/stream/stream_window.h"
#include "db/executor/stream/stream_join.h"
#include "db/executor/stream/stream_agg.h"
#include "db/stream_engine.h"
#include "db/sql/sql_executor.h"
}

/* ========================================================================
 * 测试夹具
 * ======================================================================== */

class StreamScanTest : public ::testing::Test {
protected:
    void SetUp() override {
        stream_engine_init("test_stream_dir");
        stream_engine_create("test_stream", NULL);
    }

    void TearDown() override {
        stream_engine_shutdown();
    }

    /** 向测试流中插入记录 */
    void insert_record(int64_t ts, int64_t key, double value, const char *tag) {
        stream_record_t record;
        record.timestamp = ts;
        record.key = key;
        record.value = value;
        strncpy(record.tag, tag, sizeof(record.tag) - 1);
        record.tag[sizeof(record.tag) - 1] = '\0';

        void *rel = stream_engine_open("test_stream", ACCESS_MODE_WRITE);
        ASSERT_NE(rel, nullptr);
        int rc = stream_engine_insert(rel, &record, sizeof(stream_record_t));
        ASSERT_EQ(rc, 0);
        stream_engine_close(rel);
    }

    /** 创建并填充流，返回流引擎句柄 */
    void *open_stream(const char *name) {
        return stream_engine_open(name, ACCESS_MODE_READ);
    }

    /** 释放元组槽 */
    void free_slot(TupleTableSlot *slot) {
        if (slot) {
            if (slot->tts_tupleDescriptor) {
                exec_drop_tuple_desc(slot->tts_tupleDescriptor);
            }
            exec_drop_tuple_slot(slot);
        }
    }
};

/* ========================================================================
 * 测试用例 1: StreamScanInitAndClose — 流扫描 init/close
 * ======================================================================== */
TEST_F(StreamScanTest, StreamScanInitAndClose) {
    StreamScanState *state = exec_stream_scan_init(NULL, (void *)"test_stream", 0, 100);
    ASSERT_NE(state, nullptr);

    /* 验证状态被正确初始化 */
    ASSERT_EQ(state->ps.type, EXEC_STREAM_SCAN);
    ASSERT_NE(state->ps.state, nullptr);
    ASSERT_EQ(state->watermark, 0);
    ASSERT_EQ(state->batch_size, 100);

    /* 关闭 */
    exec_stream_scan_close(state);
}

/* ========================================================================
 * 测试用例 2: StreamScanInsertAndQuery — 插入 3 条记录，扫描验证返回所有
 * ======================================================================== */
TEST_F(StreamScanTest, StreamScanInsertAndQuery) {
    /* 插入 3 条记录 */
    insert_record(1000, 1, 10.5, "tag_a");
    insert_record(2000, 2, 20.3, "tag_b");
    insert_record(3000, 3, 30.7, "tag_c");

    /* 创建流扫描 */
    StreamScanState *state = exec_stream_scan_init(NULL, (void *)"test_stream", 0, 100);
    ASSERT_NE(state, nullptr);

    /* 读取 3 条记录 */
    int count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_stream_scan_next(state)) != nullptr) {
        ASSERT_GE(slot->tts_nvalid, 3);
        int64_t ts = (int64_t)(uintptr_t)slot->tts_values[0];
        ASSERT_TRUE(ts == 1000 || ts == 2000 || ts == 3000);
        free_slot(slot);
        count++;
    }
    ASSERT_EQ(count, 3);

    exec_stream_scan_close(state);
}

/* ========================================================================
 * 测试用例 3: StreamWindowTumbling — 滚动窗口，验证窗口分组
 * ======================================================================== */
TEST_F(StreamScanTest, StreamWindowTumbling) {
    /* 插入跨多个窗口的数据 */
    /* 窗口大小 1000ms，数据分布在 [0-1000), [1000-2000), [2000-3000) 三个窗口 */
    insert_record(100, 1, 1.0, "w1");
    insert_record(500, 2, 2.0, "w1");
    insert_record(1100, 3, 3.0, "w2");
    insert_record(1500, 4, 4.0, "w2");
    insert_record(2100, 5, 5.0, "w3");

    /* 创建滚动窗口算子 */
    StreamWindowState *state = exec_stream_window_init(NULL, 1000, 1000, 0);
    ASSERT_NE(state, nullptr);

    /* 读取窗口结果 */
    int window_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_stream_window_next(state)) != nullptr) {
        ASSERT_GE(slot->tts_nvalid, 3);
        int64_t ws = (int64_t)(uintptr_t)slot->tts_values[0];
        int64_t we = (int64_t)(uintptr_t)slot->tts_values[1];
        int32_t vc = (int32_t)(int64_t)(uintptr_t)slot->tts_values[2];

        /* 验证窗口边界 */
        ASSERT_EQ(we - ws, 1000);
        /* 验证窗口内记录数 */
        if (ws == 0) {
            ASSERT_EQ(vc, 2); /* [0, 1000) 内有 2 条 */
        } else if (ws == 1000) {
            ASSERT_EQ(vc, 2); /* [1000, 2000) 内有 2 条 */
        } else if (ws == 2000) {
            ASSERT_EQ(vc, 1); /* [2000, 3000) 内有 1 条 */
        }

        free_slot(slot);
        window_count++;
    }
    ASSERT_EQ(window_count, 3);

    exec_stream_window_close(state);
}

/* ========================================================================
 * 测试用例 4: StreamWindowSliding — 滑动窗口
 * ======================================================================== */
TEST_F(StreamScanTest, StreamWindowSliding) {
    /* 插入数据 */
    insert_record(100, 1, 1.0, "s1");
    insert_record(500, 2, 2.0, "s1");
    insert_record(900, 3, 3.0, "s1");
    insert_record(1300, 4, 4.0, "s1");

    /* 创建滑动窗口：窗口大小 1000ms，滑动步长 500ms */
    StreamWindowState *state = exec_stream_window_init(NULL, 1000, 500, 1);
    ASSERT_NE(state, nullptr);

    /* 读取窗口结果 */
    int window_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_stream_window_next(state)) != nullptr) {
        ASSERT_GE(slot->tts_nvalid, 3);
        int64_t ws = (int64_t)(uintptr_t)slot->tts_values[0];
        int64_t we = (int64_t)(uintptr_t)slot->tts_values[1];
        ASSERT_EQ(we - ws, 1000);
        free_slot(slot);
        window_count++;
    }
    /* 滑动窗口应产生多个窗口：从 100 开始，每次 +500，直到窗口结束时间超过最后一条记录（1300） */
    /* 预计窗口：100-1100, 600-1600 */
    ASSERT_GE(window_count, 2);

    exec_stream_window_close(state);
}

/* ========================================================================
 * 测试用例 5: StreamJoinStreamStream — 流-流连接
 * ======================================================================== */
TEST_F(StreamScanTest, StreamJoinStreamStream) {
    /* 创建两个流 */
    stream_engine_create("stream_left", NULL);
    stream_engine_create("stream_right", NULL);

    /* 插入左流数据 */
    stream_record_t l1 = {1000, 1, 10.0, "L"};
    stream_record_t l2 = {2000, 2, 20.0, "L"};
    void *left = stream_engine_open("stream_left", ACCESS_MODE_WRITE);
    ASSERT_NE(left, nullptr);
    stream_engine_insert(left, &l1, sizeof(stream_record_t));
    stream_engine_insert(left, &l2, sizeof(stream_record_t));
    stream_engine_close(left);

    /* 插入右流数据（时间戳在左流 ±500ms 内） */
    stream_record_t r1 = {1200, 3, 30.0, "R"};  /* 匹配 l1（差 200ms） */
    stream_record_t r2 = {1800, 4, 40.0, "R"};  /* 匹配 l1（差 800ms，超出 500ms 间隔） */
    stream_record_t r3 = {2200, 5, 50.0, "R"};  /* 匹配 l2（差 200ms） */
    void *right = stream_engine_open("stream_right", ACCESS_MODE_WRITE);
    ASSERT_NE(right, nullptr);
    stream_engine_insert(right, &r1, sizeof(stream_record_t));
    stream_engine_insert(right, &r2, sizeof(stream_record_t));
    stream_engine_insert(right, &r3, sizeof(stream_record_t));
    stream_engine_close(right);

    /* 创建流连接算子（间隔 500ms） */
    StreamJoinState *state = exec_stream_join_init(NULL, 500, 0);
    ASSERT_NE(state, nullptr);

    /* 设置流名称 */
    void *internal = state->ps.state;
    ASSERT_NE(internal, nullptr);

    /* 需要通过内部机制设置流名称 */
    /* 直接使用默认名称，所以在 engine 中创建名为 stream_left 和 stream_right 的流 */
    /* 注意：stream_join_init 内部 left_name="stream_left", right_name="stream_right" */
    /* 已在上方创建了这两个流 */

    /* 读取连接结果 */
    int match_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_stream_join_next(state)) != nullptr) {
        ASSERT_GE(slot->tts_nvalid, 4);
        int64_t lts = (int64_t)(uintptr_t)slot->tts_values[0];
        int64_t rts = (int64_t)(uintptr_t)slot->tts_values[2];

        /* 验证时间差在间隔内 */
        int64_t diff = rts - lts;
        if (diff < 0) diff = -diff;
        ASSERT_LE(diff, 500);

        free_slot(slot);
        match_count++;
    }
    /* l1(1000) 匹配 r1(1200, diff=200) -> 1 条
     * l2(2000) 匹配 r2(1800, diff=200) -> 1 条
     * l2(2000) 匹配 r3(2200, diff=200) -> 1 条
     * 共计 3 条匹配 */
    ASSERT_EQ(match_count, 3);

    exec_stream_join_close(state);
}

/* ========================================================================
 * 测试用例 6: StreamAggSum — SUM 聚合
 * ======================================================================== */
TEST_F(StreamScanTest, StreamAggSum) {
    /* 插入数据：窗口 [0, 1000) 内有 [100, 500] 两条，窗口 [1000, 2000) 内有 [1100, 1500] 两条 */
    insert_record(100, 1, 10.0, "sum");
    insert_record(500, 2, 20.0, "sum");
    insert_record(1100, 3, 30.0, "sum");
    insert_record(1500, 4, 40.0, "sum");

    /* 创建 SUM 聚合算子 */
    StreamAggState *state = exec_stream_agg_init(NULL, 3, 1000);
    ASSERT_NE(state, nullptr);

    /* 读取聚合结果 */
    int window_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_stream_agg_next(state)) != nullptr) {
        ASSERT_GE(slot->tts_nvalid, 2);
        double agg = 0.0;
        memcpy(&agg, &slot->tts_values[1], sizeof(double));

        /* 验证 SUM 值 */
        if (window_count == 0) {
            ASSERT_DOUBLE_EQ(agg, 30.0); /* 10 + 20 */
        } else if (window_count == 1) {
            ASSERT_DOUBLE_EQ(agg, 70.0); /* 30 + 40 */
        }

        free_slot(slot);
        window_count++;
    }
    ASSERT_EQ(window_count, 2);

    exec_stream_agg_close(state);
}

/* ========================================================================
 * 测试用例 7: StreamAggAvg — AVG 聚合
 * ======================================================================== */
TEST_F(StreamScanTest, StreamAggAvg) {
    /* 插入数据 */
    insert_record(100, 1, 10.0, "avg");
    insert_record(500, 2, 20.0, "avg");
    insert_record(1100, 3, 30.0, "avg");
    insert_record(1500, 4, 40.0, "avg");

    /* 创建 AVG 聚合算子 */
    StreamAggState *state = exec_stream_agg_init(NULL, 0, 1000);
    ASSERT_NE(state, nullptr);

    /* 读取聚合结果 */
    int window_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_stream_agg_next(state)) != nullptr) {
        ASSERT_GE(slot->tts_nvalid, 2);
        double agg = 0.0;
        memcpy(&agg, &slot->tts_values[1], sizeof(double));

        if (window_count == 0) {
            ASSERT_DOUBLE_EQ(agg, 15.0); /* (10 + 20) / 2 */
        } else if (window_count == 1) {
            ASSERT_DOUBLE_EQ(agg, 35.0); /* (30 + 40) / 2 */
        }

        free_slot(slot);
        window_count++;
    }
    ASSERT_EQ(window_count, 2);

    exec_stream_agg_close(state);
}

/* ========================================================================
 * 测试用例 8: StreamAggMaxMin — MAX/MIN 聚合
 * ======================================================================== */
TEST_F(StreamScanTest, StreamAggMaxMin) {
    /* 插入数据 */
    insert_record(100, 1, 50.0, "mm");
    insert_record(300, 2, 10.0, "mm");
    insert_record(600, 3, 30.0, "mm");

    /* 测试 MAX */
    {
        StreamAggState *state = exec_stream_agg_init(NULL, 1, 1000);
        ASSERT_NE(state, nullptr);

        TupleTableSlot *slot = exec_stream_agg_next(state);
        ASSERT_NE(slot, nullptr);
        double agg = 0.0;
        memcpy(&agg, &slot->tts_values[1], sizeof(double));
        ASSERT_DOUBLE_EQ(agg, 50.0); /* MAX of [50, 10, 30] */
        free_slot(slot);

        exec_stream_agg_close(state);
    }

    /* 测试 MIN */
    {
        StreamAggState *state = exec_stream_agg_init(NULL, 2, 1000);
        ASSERT_NE(state, nullptr);

        TupleTableSlot *slot = exec_stream_agg_next(state);
        ASSERT_NE(slot, nullptr);
        double agg = 0.0;
        memcpy(&agg, &slot->tts_values[1], sizeof(double));
        ASSERT_DOUBLE_EQ(agg, 10.0); /* MIN of [50, 10, 30] */
        free_slot(slot);

        exec_stream_agg_close(state);
    }
}

/* ========================================================================
 * 测试用例 9: StreamEmpty — 空流扫描
 * ======================================================================== */
TEST_F(StreamScanTest, StreamEmpty) {
    /* 创建空流扫描 */
    StreamScanState *state = exec_stream_scan_init(NULL, (void *)"test_stream", 0, 100);
    ASSERT_NE(state, nullptr);

    /* 读取记录（应为空） */
    TupleTableSlot *slot = exec_stream_scan_next(state);
    ASSERT_EQ(slot, nullptr);

    exec_stream_scan_close(state);
}

/* ========================================================================
 * 测试用例 10: StreamWindowSession — 会话窗口
 * ======================================================================== */
TEST_F(StreamScanTest, StreamWindowSession) {
    /* 插入数据：两个会话
     * 会话 1: 100, 300, 500（间隙 200ms，在 gap 内）
     * 会话 2: 1500, 1700（间隙 200ms，在 gap 内，但距会话 1 超过 gap）
     */
    insert_record(100, 1, 1.0, "s1");
    insert_record(300, 2, 2.0, "s1");
    insert_record(500, 3, 3.0, "s1");
    insert_record(1500, 4, 4.0, "s2");
    insert_record(1700, 5, 5.0, "s2");

    /* 创建会话窗口（gap=600ms，会话间隙为 1000-500=500ms > 600ms 会合并）
     * 使用较小的 gap=500ms，确保两个会话分开
     */
    StreamWindowState *state = exec_stream_window_init(NULL, 500, 500, 2);
    ASSERT_NE(state, nullptr);

    /* 读取会话窗口结果 */
    int session_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_stream_window_next(state)) != nullptr) {
        ASSERT_GE(slot->tts_nvalid, 3);
        int32_t vc = (int32_t)(int64_t)(uintptr_t)slot->tts_values[2];

        if (session_count == 0) {
            ASSERT_EQ(vc, 3); /* 会话 1 有 3 条 */
        } else if (session_count == 1) {
            ASSERT_EQ(vc, 2); /* 会话 2 有 2 条 */
        }

        free_slot(slot);
        session_count++;
    }
    ASSERT_EQ(session_count, 2);

    exec_stream_window_close(state);
}

/* ========================================================================
 * 测试用例 11: StreamAggCount — COUNT 聚合
 * ======================================================================== */
TEST_F(StreamScanTest, StreamAggCount) {
    insert_record(100, 1, 5.0, "cnt");
    insert_record(300, 2, 10.0, "cnt");
    insert_record(500, 3, 15.0, "cnt");

    /* 创建 COUNT 聚合算子 */
    StreamAggState *state = exec_stream_agg_init(NULL, 4, 1000);
    ASSERT_NE(state, nullptr);

    TupleTableSlot *slot = exec_stream_agg_next(state);
    ASSERT_NE(slot, nullptr);
    double agg = 0.0;
    memcpy(&agg, &slot->tts_values[1], sizeof(double));
    ASSERT_DOUBLE_EQ(agg, 3.0); /* COUNT = 3 */
    free_slot(slot);

    exec_stream_agg_close(state);
}

/* ========================================================================
 * 测试用例 12: StreamScanWithWatermark — 带水位线的扫描
 * ======================================================================== */
TEST_F(StreamScanTest, StreamScanWithWatermark) {
    /* 插入 4 条记录，时间戳跨越不同范围 */
    insert_record(100, 1, 1.0, "wm");
    insert_record(500, 2, 2.0, "wm");
    insert_record(1000, 3, 3.0, "wm");
    insert_record(2000, 4, 4.0, "wm");

    /* 使用水位线 800，应只返回 1000 和 2000 两条 */
    StreamScanState *state = exec_stream_scan_init(NULL, (void *)"test_stream", 800, 100);
    ASSERT_NE(state, nullptr);

    int count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_stream_scan_next(state)) != nullptr) {
        int64_t ts = (int64_t)(uintptr_t)slot->tts_values[0];
        ASSERT_GT(ts, 800); /* 时间戳应大于水位线 */
        free_slot(slot);
        count++;
    }
    ASSERT_EQ(count, 2);

    exec_stream_scan_close(state);
}

/* ========================================================================
 * 测试用例 13: StreamAggWindowCross — 跨窗口聚合（验证窗口边界正确）
 * ======================================================================== */
TEST_F(StreamScanTest, StreamAggWindowCross) {
    /* 插入数据，分布在两个窗口
     * 窗口 [0, 1000): 100, 500 -> sum = 10 + 20 = 30
     * 窗口 [1000, 2000): 1100, 1500 -> sum = 30 + 40 = 70
     */
    insert_record(100, 1, 10.0, "cross");
    insert_record(500, 2, 20.0, "cross");
    insert_record(1100, 3, 30.0, "cross");
    insert_record(1500, 4, 40.0, "cross");

    /* SUM 聚合，窗口 1000ms */
    StreamAggState *state = exec_stream_agg_init(NULL, 3, 1000);
    ASSERT_NE(state, nullptr);

    int window_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_stream_agg_next(state)) != nullptr) {
        double agg = 0.0;
        memcpy(&agg, &slot->tts_values[1], sizeof(double));

        if (window_count == 0) {
            ASSERT_DOUBLE_EQ(agg, 30.0); /* 10 + 20 */
        } else if (window_count == 1) {
            ASSERT_DOUBLE_EQ(agg, 70.0); /* 30 + 40 */
        }

        free_slot(slot);
        window_count++;
    }
    ASSERT_EQ(window_count, 2);

    exec_stream_agg_close(state);
}