"""
replication — 数据库复制演示

演示主从复制 / 半同步 / GTID / 复制延迟等核心概念。
"""
import threading
import time
from typing import List, Dict, Optional
from dataclasses import dataclass, field
from enum import Enum


class ReplMode(Enum):
    ASYNC = "异步复制"
    SEMI_SYNC = "半同步复制"
    SYNC = "全同步复制"


@dataclass
class BinlogEntry:
    """二进制日志条目"""
    gtid: str
    sql: str
    timestamp: float
    applied: bool = False


class Master:
    """主库模拟"""

    def __init__(self, server_id: int):
        self.server_id = server_id
        self.data: Dict[str, int] = {"users": 0, "orders": 0}
        self.binlog: List[BinlogEntry] = []
        self.gtid_counter = 0
        self.slaves: List['Slave'] = []
        self.repl_mode = ReplMode.SEMI_SYNC
        self.ack_count = 0

    def write(self, table: str, delta: int) -> str:
        """写入数据并生成 binlog"""
        old = self.data.get(table, 0)
        self.data[table] = old + delta
        self.gtid_counter += 1
        gtid = f"{self.server_id}-{self.gtid_counter}"
        entry = BinlogEntry(gtid=gtid, sql=f"UPDATE {table} SET val={old}+{delta}",
                            timestamp=time.time())
        self.binlog.append(entry)
        print(f"  [Master] {gtid} | {entry.sql}")

        # 复制到从库
        self._replicate(entry)
        return gtid

    def _replicate(self, entry: BinlogEntry):
        self.ack_count = 0
        for slave in self.slaves:
            slave.receive(entry)

        if self.repl_mode == ReplMode.SYNC:
            # 等待所有从库确认
            while self.ack_count < len(self.slaves):
                time.sleep(0.01)
            print(f"  [Master] 全同步：{self.ack_count}/{len(self.slaves)} 确认")
        elif self.repl_mode == ReplMode.SEMI_SYNC:
            # 等待至少一个从库确认
            timeout = 0.5
            waited = 0
            while self.ack_count < 1 and waited < timeout:
                time.sleep(0.01)
                waited += 0.01
            if self.ack_count >= 1:
                print(
                    f"  [Master] 半同步：{self.ack_count}/{len(self.slaves)} 确认")
            else:
                print(f"  [Master] 半同步超时，降级为异步")
        elif self.repl_mode == ReplMode.ASYNC:
            print(f"  [Master] 异步：不等待确认")


class Slave:
    """从库模拟"""

    def __init__(self, server_id: int, lag_ms: float = 0):
        self.server_id = server_id
        self.data: Dict[str, int] = {"users": 0, "orders": 0}
        self.relay_log: List[BinlogEntry] = []
        self.lag_ms = lag_ms
        self.replied = False

    def receive(self, entry: BinlogEntry):
        """接收主库的 binlog"""
        self.relay_log.append(entry)
        # 模拟应用延迟
        if self.lag_ms > 0:
            def apply():
                time.sleep(self.lag_ms / 1000)
                entry.applied = True
                self.data.update(
                    self._parse_sql(entry.sql))
                print(
                    f"  [Slave-{self.server_id}] 应用 {entry.gtid}（延迟 {self.lag_ms}ms）")

            t = threading.Thread(target=apply, daemon=True)
            t.start()
        else:
            entry.applied = True
            self.data.update(self._parse_sql(entry.sql))
            print(f"  [Slave-{self.server_id}] 立即应用 {entry.gtid}")

    def _parse_sql(self, sql: str) -> Dict[str, int]:
        # 简单解析器
        if "users" in sql:
            return {"users": self.data.get("users", 0) + 1}
        if "orders" in sql:
            return {"orders": self.data.get("orders", 0) + 1}
        return {}


# ════════════════════════════════════════════════════════════
# 1. 异步复制
# ════════════════════════════════════════════════════════════
def demo_async():
    print("═══ 1. 异步复制（Async Replication） ═══")
    master = Master(server_id=1)
    master.repl_mode = ReplMode.ASYNC
    slave1 = Slave(server_id=101, lag_ms=0)
    slave2 = Slave(server_id=102, lag_ms=200)
    master.slaves = [slave1, slave2]

    master.write("users", 1)
    time.sleep(0.3)
    data_ok = all(s.data.get("users", 0) == 1 for s in master.slaves)
    print(f"  {'[OK]' if data_ok else '[NO]'} 从库最终一致\n")


# ════════════════════════════════════════════════════════════
# 2. 半同步复制
# ════════════════════════════════════════════════════════════
def demo_semi_sync():
    print("═══ 2. 半同步复制（Semi-Sync Replication） ═══")
    master = Master(server_id=2)
    master.repl_mode = ReplMode.SEMI_SYNC
    slave1 = Slave(server_id=201, lag_ms=0)
    slave2 = Slave(server_id=202, lag_ms=0)
    master.slaves = [slave1, slave2]

    master.write("orders", 5)
    print("  主库写入在半同步收到至少 1 个 ack 后返回\n")


# ════════════════════════════════════════════════════════════
# 3. GTID 复制
# ════════════════════════════════════════════════════════════
def demo_gtid():
    print("═══ 3. GTID（全局事务标识符） ═══")
    master = Master(server_id=3)
    master.repl_mode = ReplMode.ASYNC
    slave = Slave(server_id=301, lag_ms=0)
    master.slaves = [slave]

    gtids = []
    for i in range(3):
        gtid = master.write("users", 1)
        gtids.append(gtid)

    print(f"  主库 GTID 列表: {gtids}")
    print(f"  从库 GTID 列表: "
          f"{[e.gtid for e in slave.relay_log if e.applied]}")

    # 模拟主从切换
    print(f"\n  主从切换场景：")
    print(f"  新主库已应用最后一个 GTID: {gtids[-1]}")
    print(f"  从库已应用的 GTID: {[e.gtid for e in slave.relay_log if e.applied]}")
    print(f"  切换后数据一致性: {slave.data.get('users', 0) == 3}\n")


# ════════════════════════════════════════════════════════════
# 4. 复制延迟（Replication Lag）
# ════════════════════════════════════════════════════════════
def demo_lag():
    print("═══ 4. 复制延迟（Replication Lag） ═══")
    master = Master(server_id=4)
    master.repl_mode = ReplMode.ASYNC
    slow_slave = Slave(server_id=401, lag_ms=500)
    master.slaves = [slow_slave]

    master.write("orders", 10)
    time.sleep(0.1)  # 立即检查
    applied = slow_slave.relay_log[-1].applied if slow_slave.relay_log else False
    print(f"  写入后 100ms 检查：从库已应用 = {applied}")
    time.sleep(0.5)  # 等待应用
    applied = slow_slave.relay_log[-1].applied if slow_slave.relay_log else False
    print(f"  写入后 600ms 检查：从库已应用 = {applied}")
    print("  → 延迟容忍度：系统能否接受短时间的不一致？")
    print()


def main():
    print("=" * 50)
    print("  数据库复制演示 — 主从 / 半同步 / GTID")
    print("=" * 50 + "\n")

    demo_async()
    demo_semi_sync()
    demo_gtid()
    demo_lag()

    print("所有复制演示完成。")


if __name__ == "__main__":
    main()
