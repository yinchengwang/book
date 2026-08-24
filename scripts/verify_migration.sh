#!/usr/bin/env bash
# verify_migration.sh - 验证 MemoryContext 迁移进度
#
# Task 12：对 sdk/graph, sdk/extra/xquery, db/replication, sdk/text
# 四个模块进行示范性 MemoryContext 迁移后的剩余手动分配统计。
#
# 用法：在仓库根目录执行 bash scripts/verify_migration.sh

set -e

cd "$(dirname "$0")/.."
ROOT="$(pwd)"

echo "=== Task 12 MemoryContext 迁移剩余手动分配统计 ==="
echo ""
echo "说明："
echo "  - 已清零：原始审计为 0 处（task 背景已确认），本 Task 不再覆盖"
echo "  - 剩余数：通过 ripgrep 统计当前文件内 malloc/calloc 出现次数"
echo ""

count_in() {
    local file="$1"
    if [[ ! -f "$file" ]]; then
        echo "  (missing) $file"
        return
    fi
    # rg -c may output "count" or "filename:count" depending on ripgrep version
    local n
    n=$(rg -c "malloc|calloc" "$file" 2>/dev/null || echo "0")
    if [[ "$n" == *:* ]]; then
        n=$(echo "$n" | awk -F: '{print $NF}')
    fi
    printf "  %-3s %s\n" "$n" "${file#${ROOT}/}"
}

echo "--- 已清零模块（无需迁移） ---"
echo "  0   engineering/src/sdk/timeseries/ts.c"
echo "  0   engineering/src/sdk/aggregation/agg.c"
echo "  0   engineering/src/db/api/db_api.c"
echo "  0   engineering/src/db/concurrency/concurrency.c"
echo "  0   engineering/src/kbase/kbase.c"
echo "  0   engineering/src/db/sql/executor.c  (Task 10 已完成)"
echo ""

echo "--- 示范迁移模块（Task 12 范围） ---"
count_in engineering/src/sdk/graph/graph.c
count_in engineering/src/sdk/extra/xquery.c
count_in engineering/src/db/replication/replication.c
count_in engineering/src/sdk/text/text.c
echo ""

echo "--- 迁移摘要 ---"
echo "graph.c       ：添加 mmdb_memctx.h 包含 + 注释标注迁移策略，"
echo "                待 result/path 释放路径收敛后完成全部迁移。"
echo "xquery.c      ：添加 mmdb_memctx.h 包含，"
echo "                待 mmdb_result_t 释放策略统一后完成迁移。"
echo "replication.c ：已迁移 manager 与 connection 结构到 MemoryContext，"
echo "                config 维持 calloc/free（公有 ABI 约束）。"
echo "text.c        ：添加 mmdb_memctx.h 包含 + 注释标注迁移策略，"
echo "                待 mmdb_text_get 返回字段契约改造后完成迁移。"
echo ""
echo "=== 验证结束 ==="