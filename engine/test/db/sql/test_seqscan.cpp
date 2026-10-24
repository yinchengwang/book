/**
 * @file test_seqscan.cpp
 * @brief SeqScan 执行器节点单元测试（新 Volcano 阵营，T8 重写）
 *
 * T8 将 SeqScan 统一到新阵营（include/db/sql/nodeSeqscan.h +
 * execnodes.h/executor.h 的 Plan/EState/TupleTableSlot 世界），
 * 旧阵营（sql_executor.h 的 SeqScanPlan/SeqScanExtState）实现已删除，
 * 本文件随之重写：
 *   - 执行器状态经 CreateEState/FreeEState 管理；
 *   - 无表节点（scanrelid=0）行为；
 *   - 行编解码（sql_row_encode/decode）往返与边界；
 *   - 真实表端到端扫描（catalog + heapam + SeqScan 解码）。
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/rel.h"              /* 必须先于 executor 头（TupleDescData 真身） */
#include "db/sql/nodeSeqscan.h"
#include "db/sql/executor.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/btreeam.h"
}

namespace {

/* PG 类型 OID（与 nodeSeqscan.c 支持的编解码类型一致） */
constexpr uint32_t kOidInt2 = 21;
constexpr uint32_t kOidInt4 = 23;
constexpr uint32_t kOidInt8 = 20;
constexpr uint32_t kOidText = 25;
constexpr uint32_t kOidBool = 16;  /* 编解码不支持的类型代表 */

/* ====================================================================
 * 无表节点（scanrelid=0）：生命周期与 NULL 容错
 * ==================================================================*/

TEST(SeqScanTest, InitWithoutRelation) {
    SeqScan plan;
    memset(&plan, 0, sizeof(plan));
    plan.plan.type = T_SeqScan;
    plan.scanrelid = 0;  /* 无表占位节点 */

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    PlanState *ps = ExecInitSeqScan((Plan *)&plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_SeqScanState);
    EXPECT_EQ(ps->ExecProcNode, ExecSeqScan);
    EXPECT_EQ(ps->state, estate);

    SeqScanState *ss = (SeqScanState *)ps;
    EXPECT_EQ(ss->ss_currentRelation, nullptr);
    EXPECT_EQ(ss->ss_currentScanDesc, nullptr);

    /* 无 Relation：任何次数调用都返回 NULL（0 行） */
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(ExecProcNode(ps), nullptr);
    }

    ExecEndSeqScan(ss);
    FreeEState(estate);
}

TEST(SeqScanTest, NullParameters) {
    EXPECT_EQ(ExecInitSeqScan(nullptr, nullptr, 0), nullptr);
    EXPECT_EQ(ExecSeqScan(nullptr), nullptr);
    EXPECT_NO_FATAL_FAILURE(ExecEndSeqScan(nullptr));
    EXPECT_NO_FATAL_FAILURE(ExecReScanSeqScan(nullptr));
}

TEST(SeqScanTest, InitRejectsWrongPlanTag) {
    SeqScan plan;
    memset(&plan, 0, sizeof(plan));
    plan.plan.type = T_Result;  /* 非 T_SeqScan：必须拒绝 */
    plan.scanrelid = 0;

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);
    EXPECT_EQ(ExecInitSeqScan((Plan *)&plan, estate, 0), nullptr);
    FreeEState(estate);
}

/* ====================================================================
 * 行编解码
 * ==================================================================*/

class SeqScanCodecTest : public ::testing::Test {
protected:
    MemoryContext mcxt_ = nullptr;
    MemoryContext old_ = nullptr;

    void SetUp() override {
        mcxt_ = AllocSetContextCreate(nullptr, "SeqScanCodecTest", 0,
                                      ALLOCSET_DEFAULT_BLOCK_SIZE,
                                      ALLOCSET_DEFAULT_BLOCK_SIZE,
                                      ALLOCSET_PRESET_DEFAULT);
        ASSERT_NE(mcxt_, nullptr);
        old_ = MemoryContextSwitchTo(mcxt_);
    }
    void TearDown() override {
        MemoryContextSwitchTo(old_);
        delete_memory(mcxt_);
    }
};

TEST_F(SeqScanCodecTest, RoundTripMixedTypes) {
    Oid types[4] = {kOidInt2, kOidInt4, kOidInt8, kOidText};
    const char *text = "hello t8";
    Datum values[4] = {
        (Datum)(uint64_t)(int64_t)(int16_t)-7,
        (Datum)(uint64_t)(int64_t)(int32_t)-123456,
        (Datum)(uint64_t)(int64_t)(-9000000000LL),
        (Datum)(uintptr_t)text,
    };
    bool isnull[4] = {false, false, false, false};

    size_t need = sql_row_encoded_size(4, types, values, isnull);
    ASSERT_GT(need, 0u);

    uint8_t buf[256];
    size_t out_len = 0;
    ASSERT_EQ(sql_row_encode(4, types, values, isnull,
                             buf, sizeof(buf), &out_len), 0);
    EXPECT_EQ(out_len, need);

    Datum dv[4] = {0, 0, 0, 0};
    bool dn[4] = {true, true, true, true};
    ASSERT_EQ(sql_row_decode(4, types, buf, dv, dn, mcxt_), 0);

    EXPECT_FALSE(dn[0]);
    EXPECT_EQ((int16_t)(int64_t)dv[0], -7) << "int2 必须符号扩展还原";
    EXPECT_FALSE(dn[1]);
    EXPECT_EQ((int32_t)(int64_t)dv[1], -123456) << "int4 必须符号扩展还原";
    EXPECT_FALSE(dn[2]);
    EXPECT_EQ((int64_t)dv[2], -9000000000LL) << "int8 必须原值还原";
    EXPECT_FALSE(dn[3]);
    EXPECT_STREQ((const char *)(uintptr_t)dv[3], "hello t8");
}

TEST_F(SeqScanCodecTest, NullColumns) {
    Oid types[2] = {kOidInt4, kOidText};
    Datum values[2] = {(Datum)0, (Datum)0};
    bool isnull[2] = {true, true};

    size_t need = sql_row_encoded_size(2, types, values, isnull);
    ASSERT_GT(need, 0u);
    uint8_t buf[64];
    size_t out_len = 0;
    ASSERT_EQ(sql_row_encode(2, types, values, isnull,
                             buf, sizeof(buf), &out_len), 0);

    Datum dv[2] = {123, 456};  /* 解码必须将其覆盖 */
    bool dn[2] = {false, false};
    ASSERT_EQ(sql_row_decode(2, types, buf, dv, dn, mcxt_), 0);
    EXPECT_TRUE(dn[0]);
    EXPECT_TRUE(dn[1]);
    EXPECT_EQ(dv[0], (Datum)0);
    EXPECT_EQ(dv[1], (Datum)0);
}

TEST_F(SeqScanCodecTest, UnsupportedTypeFailsExplicitly) {
    Oid types[1] = {kOidBool};  /* 编解码不支持 */
    Datum values[1] = {(Datum)1};
    bool isnull[1] = {false};

    EXPECT_EQ(sql_row_encoded_size(1, types, values, isnull), 0u);

    uint8_t buf[32] = {0};
    size_t out_len = 0;
    EXPECT_EQ(sql_row_encode(1, types, values, isnull,
                             buf, sizeof(buf), &out_len), -1);
}

TEST_F(SeqScanCodecTest, CorruptBlobRejected) {
    Oid types[2] = {kOidInt4, kOidText};
    const char *text = "abcdef";
    Datum values[2] = {(Datum)(uint64_t)(int64_t)42, (Datum)(uintptr_t)text};
    bool isnull[2] = {false, false};

    uint8_t buf[128];
    size_t out_len = 0;
    ASSERT_EQ(sql_row_encode(2, types, values, isnull,
                             buf, sizeof(buf), &out_len), 0);

    /* 截断 total_len 前缀：text 长度越界，必须拒绝而非越界读 */
    uint32_t truncated = 4 + 1 + 4 + 1 + 2;  /* 只够 text 长度前缀的 2 字节 */
    memcpy(buf, &truncated, sizeof(truncated));

    Datum dv[2] = {0, 0};
    bool dn[2] = {false, false};
    EXPECT_EQ(sql_row_decode(2, types, buf, dv, dn, mcxt_), -1);
}

/* ====================================================================
 * 真实表端到端扫描
 * ==================================================================*/

class SeqScanTableTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(catalog_init(), 0);
        ASSERT_EQ(buf_init(""), 0);
        ASSERT_EQ(heapam_init(), 0);
        ASSERT_EQ(btreeam_init(), 0);
        ASSERT_EQ(rel_init(), 0);
    }
    static void TearDownTestSuite() {
        rel_shutdown();
        btreeam_shutdown();
        heapam_shutdown();
        buf_shutdown();
        catalog_shutdown();
    }
};

TEST_F(SeqScanTableTest, ScanRealTableDecodesRows) {
    /* 建表（catalog 直通道，与驱动 DDL 同路径） */
    column_def_t cols[2];
    memset(cols, 0, sizeof(cols));
    snprintf(cols[0].name, NAMEDATALEN, "id");
    cols[0].type_oid = kOidInt4;
    snprintf(cols[1].name, NAMEDATALEN, "name");
    cols[1].type_oid = kOidText;
    Oid table_oid = catalog_create_table("seqscan_ut_t", cols, 2);
    ASSERT_NE(table_oid, 0u);

    /* 写入两行（行编码与 SeqScan 解码同源） */
    Relation wrel = relation_open(table_oid, REL_OPEN_READWRITE);
    ASSERT_NE(wrel, nullptr);
    TupleDesc wtd = relation_getdesc(wrel);
    ASSERT_NE(wtd, nullptr);
    ASSERT_EQ(wtd->natts, 2);
    Oid coltypes[2] = {wtd->attrs[0].atttypid, wtd->attrs[1].atttypid};

    /* 描述符列序不保证等于声明顺序：按 attname 定位目标 attno */
    int att_id = -1, att_name = -1;
    for (int i = 0; i < wtd->natts; i++) {
        if (strcmp(wtd->attrs[i].attname, "id") == 0) att_id = i;
        if (strcmp(wtd->attrs[i].attname, "name") == 0) att_name = i;
    }
    ASSERT_GE(att_id, 0);
    ASSERT_GE(att_name, 0);

    const char *names[2] = {"alice", "bob"};
    for (int r = 0; r < 2; r++) {
        Datum values[2] = {0, 0};
        bool isnull[2] = {false, false};
        values[att_id] = (Datum)(uint64_t)(int64_t)(r + 1);
        values[att_name] = (Datum)(uintptr_t)names[r];
        size_t need = sql_row_encoded_size(2, coltypes, values, isnull);
        ASSERT_GT(need, 0u);
        uint8_t buf[128];
        size_t out_len = 0;
        ASSERT_EQ(sql_row_encode(2, coltypes, values, isnull,
                                 buf, sizeof(buf), &out_len), 0);
        ASSERT_EQ(heap_insert(wrel, buf, out_len, 0, 0, NULL, NULL), 0);
    }
    relation_close(wrel, 0);

    /* SeqScan 全表扫描 */
    SeqScan plan;
    memset(&plan, 0, sizeof(plan));
    plan.plan.type = T_SeqScan;
    plan.scanrelid = table_oid;

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);
    PlanState *ps = ExecInitSeqScan((Plan *)&plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    SeqScanState *ss = (SeqScanState *)ps;
    ASSERT_EQ(ss->ss_natts, 2);

    int rows = 0;
    bool saw_alice = false, saw_bob = false;
    TupleTableSlot *slot;
    while ((slot = ExecProcNode(ps)) != nullptr) {
        ASSERT_EQ(slot->tts_nvalid, 2);
        /* 按 attname 定位列（描述符列序不保证等于声明顺序） */
        int idx_id = -1, idx_name = -1;
        TupleDesc td = slot->tts_tupleDescriptor;
        for (int i = 0; i < td->natts; i++) {
            if (strcmp(td->attrs[i].attname, "id") == 0) idx_id = i;
            if (strcmp(td->attrs[i].attname, "name") == 0) idx_name = i;
        }
        ASSERT_GE(idx_id, 0);
        ASSERT_GE(idx_name, 0);
        EXPECT_FALSE(slot->tts_isnull[idx_id]);
        EXPECT_FALSE(slot->tts_isnull[idx_name]);
        const char *name = (const char *)(uintptr_t)slot->tts_values[idx_name];
        if (strcmp(name, "alice") == 0) {
            saw_alice = true;
            EXPECT_EQ((int32_t)(int64_t)slot->tts_values[idx_id], 1);
        } else if (strcmp(name, "bob") == 0) {
            saw_bob = true;
            EXPECT_EQ((int32_t)(int64_t)slot->tts_values[idx_id], 2);
        }
        rows++;
    }
    EXPECT_EQ(rows, 2) << "必须扫出且仅扫出 2 行";
    EXPECT_TRUE(saw_alice);
    EXPECT_TRUE(saw_bob);

    /* ReScan：再次全量返回 */
    ExecReScanSeqScan(ss);
    int rescan_rows = 0;
    while (ExecProcNode(ps) != nullptr) {
        rescan_rows++;
    }
    EXPECT_EQ(rescan_rows, 2);

    ExecEndSeqScan(ss);
    FreeEState(estate);

    ASSERT_EQ(catalog_drop_table(table_oid), CATALOG_SUCCESS);
}

}  /* namespace */
