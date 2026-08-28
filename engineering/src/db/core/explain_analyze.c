/**
 * @file explain_analyze.c
 * @brief EXPLAIN/ANALYZE 执行器钩子（C2-2 T3/T6）
 */
#include "db/explain_analyze.h"
#include "db/core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 前向声明 */
void vacuum_trigger_check(void);

int sql_analyze_table(const char *table_name) {
    /* C2-2 T3：ANALYZE 采样填充 AttStats（占位）
     * 完整实现：扫描 heap → 采样 30000 行 → 计算 ndistinct / histogram / MCV → 写 catalog
     * 当前骨架：触发 vacuum_trigger_check 完成部分清理逻辑
     */
    LOG_INFO("ANALYZE %s：触发 vacuum trigger + skeleton AttStats 填充", table_name);
    vacuum_trigger_check();
    return 0;
}

char *sql_explain_plan(const void *planstate) {
    /* C2-2 T6：EXPLAIN 输出（占位）
     * 完整实现：遍历 planstate 节点，输出 SeqScan/IndexScan/HashJoin/NL 等 + 估算行数 + 代价
     * 当前骨架：基础格式 + 估算行数显示
     */
    if (!planstate) {
        return strdup("EXPLAIN: 空计划\n");
    }
    /* 占位输出：节点类型 + 估算行数 1000（RC 默认 ndistinct 1） */
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "EXPLAIN:\n"
             "  -> 节点类型: 占位 PlanState (%p)\n"
             "     估算行数: 1000\n"
             "     估算代价: 0.00 (RC 默认)\n",
             planstate);
    return strdup(buf);
}