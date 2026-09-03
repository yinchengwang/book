"""
sharding — 分库分表（Sharding）演示

演示水平分片 / 垂直分片 / 路由策略 / 跨分片查询。
"""

import hashlib
from typing import List, Dict, Any, Callable, Optional


# ════════════════════════════════════════════════════════════
# 分片模拟器
# ════════════════════════════════════════════════════════════
class Shard:
    """单个分片，模拟一个数据库实例"""

    def __init__(self, name: str):
        self.name = name
        self.rows: List[Dict[str, Any]] = []

    def insert(self, row: Dict[str, Any]):
        self.rows.append(row)

    def query_all(self) -> List[Dict[str, Any]]:
        return self.rows

    def __repr__(self):
        return f"Shard({self.name}, {len(self.rows)} rows)"


class ShardRouter:
    """分片路由器"""

    def __init__(self, shards: List[Shard],
                 strategy: str = "hash",
                 shard_col: str = "user_id"):
        self.shards = shards
        self.strategy = strategy
        self.shard_col = shard_col

    def _hash_shard(self, key) -> int:
        """Hash 分片：hash(key) % N"""
        h = hashlib.md5(str(key).encode()).hexdigest()
        return int(h, 16) % len(self.shards)

    def _range_shard(self, key) -> int:
        """范围分片"""
        if isinstance(key, int):
            if key <= 333:
                return 0
            elif key <= 666:
                return 1
            return 2
        return 0

    def get_shard(self, row: Dict[str, Any]) -> Shard:
        """根据分片策略和目标行确定分片"""
        val = row.get(self.shard_col, 0)
        if self.strategy == "hash":
            idx = self._hash_shard(val)
        elif self.strategy == "range":
            idx = self._range_shard(val)
        else:
            idx = 0
        return self.shards[idx]

    def insert(self, row: Dict[str, Any]):
        shard = self.get_shard(row)
        shard.insert(row)

    def query_all(self) -> List[Dict[str, Any]]:
        """跨分片查询（合并结果）"""
        results = []
        for shard in self.shards:
            results.extend(shard.query_all())
        return results


# ════════════════════════════════════════════════════════════
# 1. 水平分片（Horizontal Sharding）
# ════════════════════════════════════════════════════════════
def demo_horizontal():
    print("═══ 1. 水平分片（Horizontal Sharding） ═══")
    shards = [Shard("shard-0"), Shard("shard-1"), Shard("shard-2")]

    # Hash 分片
    router = ShardRouter(shards, strategy="hash", shard_col="user_id")
    for uid in range(1, 13):
        router.insert({"user_id": uid, "name": f"User{uid}", "city": "北京"})

    for s in shards:
        print(f"  {s.name}: {[r['user_id'] for r in s.rows]}")
    print("  数据按 user_id hash 均匀散落到 3 个分片\n")


# ════════════════════════════════════════════════════════════
# 2. 垂直分片（Vertical Sharding）
# ════════════════════════════════════════════════════════════
def demo_vertical():
    print("═══ 2. 垂直分片（Vertical Sharding） ═══")
    user_base = Shard("shard-user-base")
    user_ext = Shard("shard-user-ext")

    # 用户基本信息
    user_base.insert(
        {"user_id": 1, "name": "Alice", "age": 30, "gender": "F"})
    user_base.insert(
        {"user_id": 2, "name": "Bob", "age": 25, "gender": "M"})

    # 用户扩展信息（大字段/低频访问）
    user_ext.insert({"user_id": 1, "bio": "软件工程师",
                    "avatar": "alice.jpg", "settings": "dark_mode"})
    user_ext.insert({"user_id": 2, "bio": "产品经理",
                    "avatar": "bob.jpg", "settings": "light_mode"})

    print(f"  {user_base.name}: 基本信息表（高频访问）")
    print(f"  {user_ext.name}: 扩展信息表（低频/大字段）")
    print("  → 垂直分片：按列拆分到不同数据库\n")


# ════════════════════════════════════════════════════════════
# 3. 范围分片（Range Sharding）—— 订单按时间
# ════════════════════════════════════════════════════════════
def demo_range():
    print("═══ 3. 范围分片 ═══")
    shards = [Shard("shard-q1"), Shard("shard-q2"), Shard("shard-q3")]

    orders = [
        ("2026-01-15", "Order-001", 100),
        ("2026-02-20", "Order-002", 200),
        ("2026-04-10", "Order-003", 150),
        ("2026-07-05", "Order-004", 300),
        ("2026-09-01", "Order-005", 250),
        ("2026-11-30", "Order-006", 180),
    ]

    # 按季度取模分片
    for date, oid, amt in orders:
        month = int(date.split("-")[1])
        idx = (month - 1) // 4  # Q1=0, Q2=1, Q3=2
        shards[idx].insert(
            {"order_id": oid, "date": date, "amount": amt})

    for s in shards:
        print(f"  {s.name}: {[r['order_id'] for r in s.rows]}")
    print("  → 按日期/季度范围分片，便于按时间维度查询\n")


# ════════════════════════════════════════════════════════════
# 4. 跨分片查询（分布式查询）
# ════════════════════════════════════════════════════════════
def demo_cross_shard():
    print("═══ 4. 跨分片查询与聚合 ═══")
    shards = [Shard("shard-0"), Shard("shard-1"), Shard("shard-2")]
    router = ShardRouter(shards, strategy="hash", shard_col="user_id")

    for uid in range(1, 101):
        router.insert({"user_id": uid, "score": uid * 10})

    # 跨分片查询：全局统计
    all_data = router.query_all()
    total_score = sum(r["score"] for r in all_data)
    avg_score = total_score / len(all_data) if all_data else 0

    print(f"  总分: {total_score}, 平均分: {avg_score:.1f}")
    print(f"  (= 100 个用户的 score 之和，跨 3 个分片汇总)\n")


# ════════════════════════════════════════════════════════════
# 5. 分片键选择
# ════════════════════════════════════════════════════════════
def demo_shard_key():
    print("═══ 5. 分片键选择原则 ═══")
    principles = [
        "均匀性：分片键的取值分布应均匀（如 user_id，而非 gender）",
        "业务关联：相同分片键的事务应在同一分片（避免分布式事务）",
        "不可变性：分片键不应频繁更新（更改分片需迁移数据）",
        "查询友好：大部分查询带分片键条件 → 精确定位单个分片",
    ]
    for i, p in enumerate(principles, 1):
        print(f"  {i}. {p}")
    print()


def main():
    print("=" * 50)
    print("  分库分表演示 — 水平/垂直/范围/跨分片查询")
    print("=" * 50 + "\n")

    demo_horizontal()
    demo_vertical()
    demo_range()
    demo_cross_shard()
    demo_shard_key()

    print("所有分片演示完成。")


if __name__ == "__main__":
    main()
