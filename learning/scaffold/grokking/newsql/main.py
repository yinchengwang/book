"""
newsql — NewSQL 新一代数据库演示

演示 TiDB / CockroachDB 风格的分布式 SQL + HTAP 混合负载。
"""

import threading
import time
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field
import random


# ════════════════════════════════════════════════════════════
# TiDB 架构模拟
# ════════════════════════════════════════════════════════════
@dataclass
class Region:
    """TiDB Region：数据分片单元"""
    id: int
    table: str
    start_key: str
    end_key: str
    data: Dict[str, Dict] = field(default_factory=dict)

    def contains(self, key: str) -> bool:
        return self.start_key <= key < self.end_key


class PD:
    """Placement Driver：元数据管理、全局时钟"""

    def __init__(self):
        self.regions: Dict[int, Region] = {}
        self.ts: int = 0

    def next_ts(self) -> int:
        self.ts += 1
        return self.ts


class TiKV:
    """TiKV 存储节点"""

    def __init__(self, node_id: str):
        self.node_id = node_id
        self.regions: List[Region] = []


class TiDBServer:
    """TiDB 计算节点（SQL 层）"""

    def __init__(self, pd: PD, tikv_nodes: List[TiKV]):
        self.pd = pd
        self.tikv_nodes = tikv_nodes

    def execute_query(self, sql: str) -> str:
        """模拟 SQL 执行——下推到 TiKV 并行执行"""
        print(f"  [TiDB] 解析: {sql}")
        print(f"  [TiDB] 生成执行计划（分布式）")
        print(f"  [TiDB] 下推到 {len(self.tikv_nodes)} TiKV 节点并行执行")
        time.sleep(0.1)
        # 模拟 Coprocessor 结果合并
        print(f"  [TiDB] Coprocessor 结果 Merge 完成")
        return f"OK ({len(self.tikv_nodes)} rows)"


# ════════════════════════════════════════════════════════════
# CockroachDB 风格演示：Raft + 向量时钟
# ════════════════════════════════════════════════════════════
class CRDBRange:
    """CockroachDB Range：Raft 复制组"""

    def __init__(self, range_id: int, replicas: int = 3):
        self.range_id = range_id
        self.replicas = replicas
        self.leader = 0
        self.data: Dict[str, str] = {}
        self.term = 0

    def write(self, key: str, value: str) -> bool:
        """Raft 写入——Leader 收到多数派确认后提交"""
        self.term += 1
        self.data[key] = value
        ack_count = self.replicas - 1  # 模拟副本复制成功
        print(f"    Raft Range-{self.range_id}: term={self.term}")
        return ack_count >= (self.replicas // 2 + 1)


# ════════════════════════════════════════════════════════════
# 1. TiDB 架构
# ════════════════════════════════════════════════════════════
def demo_tidb():
    print("═══ 1. TiDB 架构 ═══")
    pd = PD()
    tikvs = [TiKV("tikv-1"), TiKV("tikv-2"), TiKV("tikv-3")]
    tidb = TiDBServer(pd, tikvs)

    # 创建 Region
    for i in range(3):
        r = Region(id=i + 1, table="users",
                   start_key=f"User-{i*100}", end_key=f"User-{(i+1)*100}")
        for j in range(i * 100, (i + 1) * 100):
            r.data[f"User-{j}"] = {"name": f"User{j}", "age": 20 + j % 40}
        pd.regions[r.id] = r
        tikvs[i].regions.append(r)
        print(f"  Region-{r.id}: User-{i*100} ~ User-{(i+1)*100} → tikv-{i+1}")

    result = tidb.execute_query("SELECT * FROM users WHERE age > 30")
    print(f"  结果: {result}")
    print(f"  → TiDB = 计算存储分离 + 水平扩展 + MySQL 兼容\n")


# ════════════════════════════════════════════════════════════
# 2. CockroachDB 架构
# ════════════════════════════════════════════════════════════
def demo_crdb():
    print("═══ 2. CockroachDB 架构 ═══")
    ranges = [CRDBRange(i + 1) for i in range(3)]

    for i, r in enumerate(ranges):
        ok = r.write(f"key_{i}", f"value_{i}")
        print(f"  Range-{r.range_id} 写入: {'[OK]' if ok else '[NO]'}")

    print("  → CockroachDB = Raft 共识 + 自动分片 + 强一致")
    print("  → 每个 Range 是一个 Raft 组（默认 3 副本）")
    print()


# ════════════════════════════════════════════════════════════
# 3. HTAP 混合负载
# ════════════════════════════════════════════════════════════
def demo_htap():
    print("═══ 3. HTAP — 混合事务/分析处理 ═══")

    # 行存 (TP)
    print("  [行存] OLTP 负载：点查询、小范围扫描")
    print("    SELECT * FROM orders WHERE id = 12345")
    print("    INSERT INTO orders VALUES (...)")
    print("    — 按主键聚簇存储，一页存整行")

    # 列存 (AP)
    print("\n  [列存] OLAP 负载：大表扫描、聚合")
    print("    SELECT region, SUM(amount) FROM orders GROUP BY region")
    print("    — 按列存储，只读需要的列（列裁剪）")

    # HTAP 统一
    print("\n  [HTAP] 行存 + 列存同时存在")
    print("    - 写入实时同步到行存和列存")
    print("    - TP 查询走行存（单行快），AP 查询走列存（扫描快）")
    print("    - 代表：TiFlash（TiDB 的列存引擎）")
    print()


# ════════════════════════════════════════════════════════════
# 4. 分布式 SQL vs 单体 SQL
# ════════════════════════════════════════════════════════════
def demo_distributed_vs_monolithic():
    print("═══ 4. 分布式 SQL vs 单体 SQL ═══")
    print("  ┌─────────────┬─────────────────┬─────────────────┐")
    print("  │ 特性        │ 单体 SQL (MySQL)│ 分布式 SQL (TiDB)│")
    print("  ├─────────────┼─────────────────┼─────────────────┤")
    print("  │ 扩展性      │ 垂直扩展        │ 水平扩展         │")
    print("  │ 存储上限    │ 单机磁盘        │ 理论上无限       │")
    print("  │ 写入吞吐    │ 受物理机限制    │ 随节点线性增长   │")
    print("  │ 故障影响    │ 单点故障        │ 多副本容忍 N-1   │")
    print("  │ 分布式事务  │ 不支持          │ 2PC （悲观/乐观）│")
    print("  │ 数据分片    │ 手动分库分表    │ 自动分裂          │")
    print("  │ 一致性      │ ACID            │ 强一致/最终       │")
    print("  └─────────────┴─────────────────┴─────────────────┘")
    print()


def main():
    print("=" * 50)
    print("  NewSQL 演示 — TiDB / CockroachDB / HTAP")
    print("=" * 50 + "\n")

    demo_tidb()
    demo_crdb()
    demo_htap()
    demo_distributed_vs_monolithic()

    print("所有 NewSQL 演示完成。")


if __name__ == "__main__":
    main()
