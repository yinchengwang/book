"""
数据库设计 — 主从复制 + 分库分表 + 读写分离

演示内容:
1. 主从复制: 同步/异步复制 + 复制延迟
2. 分库分表: 水平分片 + 路由策略
3. 读写分离: 读流量分发 + 一致性保证
"""

import hashlib
import random
import time

random.seed(42)

# ============================================================================
# 1. 主从复制
# ============================================================================

class BinlogEntry:
    """Binlog 条目模拟"""

    def __init__(self, lsn: int, sql: str):
        self.lsn = lsn
        self.sql = sql
        self.created_at = time.time()


class MySQLReplication:
    """MySQL 主从复制模拟"""

    def __init__(self):
        self.master_log = []
        self.replicas = {f"replica-{i}": [] for i in range(3)}
        self.current_lsn = 0
        self.replica_lag = {}  # replica → lag(条数)

    def write(self, sql: str) -> BinlogEntry:
        """主库写入"""
        self.current_lsn += 1
        entry = BinlogEntry(self.current_lsn, sql)
        self.master_log.append(entry)
        return entry

    def sync_replicate(self):
        """同步复制: 等待所有从库确认"""
        for replica in self.replicas:
            self.replicas[replica] = list(self.master_log)
            self.replica_lag[replica] = 0

    def async_replicate(self, lag_random: bool = True):
        """异步复制: 从库可能有延迟"""
        for replica in self.replicas:
            if lag_random:
                # 模拟随机延迟: 落后 0~5 条
                lag = random.randint(0, 5)
                sync_point = max(0, len(self.master_log) - lag)
                self.replicas[replica] = self.master_log[:sync_point]
                self.replica_lag[replica] = lag
            else:
                self.replicas[replica] = list(self.master_log)
                self.replica_lag[replica] = 0

    def status(self) -> dict:
        return {
            "master_log_size": len(self.master_log),
            "replica_lag": dict(self.replica_lag),
        }


# ============================================================================
# 2. 分库分表
# ============================================================================

class ShardRouter:
    """分片路由 — 按 hash 取模"""

    def __init__(self, db_count: int, table_count: int):
        self.db_count = db_count
        self.table_count = table_count

    def _hash(self, key: str) -> int:
        return int(hashlib.md5(key.encode()).hexdigest(), 16)

    def route(self, key: str) -> dict:
        """计算 key 的路由目标"""
        h = self._hash(key)
        db_idx = h % self.db_count
        table_idx = (h // self.db_count) % self.table_count
        return {
            "db": f"db_{db_idx:02d}",
            "table": f"t_{table_idx:02d}",
            "db_idx": db_idx,
            "table_idx": table_idx,
        }


# ============================================================================
# 3. 读写分离
# ============================================================================

class ReadWriteSplitting:
    """读写分离路由"""

    def __init__(self, replicas_count: int = 3):
        self.replicas = [f"replica-{i}" for i in range(replicas_count)]
        self.current_idx = 0

    def write(self) -> str:
        """写操作 → 主库"""
        return "master"

    def read(self) -> str:
        """读操作 → 轮询从库"""
        replica = self.replicas[self.current_idx]
        self.current_idx = (self.current_idx + 1) % len(self.replicas)
        return replica

    def read_with_consistency(self, is_critical: bool = False) -> str:
        """强一致性读 → 主库"""
        return "master" if is_critical else self.read()


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("    数据库设计演示 — Database Architecture")
    print("=" * 56)

    print("\n--- 1. 主从复制 ---")
    repl = MySQLReplication()
    for i in range(10):
        repl.write(f"INSERT INTO users VALUES ({i}, 'user{i}')")
    repl.sync_replicate()
    print(f"  同步复制 → 从库延迟: {repl.status()['replica_lag']}")

    repl.async_replicate()
    print(f"  异步复制 → 从库延迟: {repl.status()['replica_lag']}")
    print(f"  主库日志: {repl.status()['master_log_size']} 条")

    print("\n--- 2. 分库分表 ---")
    router = ShardRouter(db_count=8, table_count=4)
    user_ids = [f"user_{i}" for i in range(20)]
    print(f"{'用户ID':<16} {'数据库':<10} {'表':<10}")
    print("-" * 38)
    for uid in user_ids:
        r = router.route(uid)
        print(f"{uid:<16} {r['db']:<10} {r['table']:<10}")

    # 统计分布
    from collections import Counter
    db_dist = Counter(router.route(uid)["db"] for uid in
                      [f"user_{i}" for i in range(10000)])
    print(f"\n  10000 用户 DB 分布: ")
    for db, cnt in sorted(db_dist.items()):
        print(f"    {db}: {cnt} ({cnt / 100:.1f}%)")

    print("\n--- 3. 读写分离 ---")
    rws = ReadWriteSplitting()
    for i in range(8):
        op = "写" if i % 4 == 0 else "读"
        target = rws.write() if op == "写" else rws.read()
        ch = "[OK]" if target != "master" else ""
        print(f"  {op}操作 → {target} {ch}")
    print(f"  强一致性读 → {rws.read_with_consistency(True)} (强制走主库)")

    print("\n" + "=" * 56)
    print("提示: 分库分表后跨节点 JOIN / 事务需特殊处理")
    print("=" * 56)
