"""
分布式同步 — 分布式锁 + 乐观锁 + 悲观锁

演示内容:
1. 分布式锁: 基于 Redis 的 SET NX + 租约
2. 乐观锁: CAS (Compare-And-Swap) 版本号
3. 悲观锁: 数据库行锁 + 两阶段锁
"""

import threading
import time
from typing import Optional

# ============================================================================
# 1. 分布式锁
# ============================================================================

class DistributedLock:
    """基于租约的分布式锁（教学简化）"""

    def __init__(self, lock_name: str, lease_ms: int = 5000):
        self.lock_name = lock_name
        self.lease_ms = lease_ms
        self._holder = None     # 当前持有者
        self._expires_at = 0    # 锁过期时间
        self._lock = threading.Lock()

    def acquire(self, holder: str, timeout_ms: int = 3000) -> bool:
        """获取锁（类似 Redis SET NX EX）"""
        deadline = time.time() + timeout_ms / 1000
        while time.time() < deadline:
            with self._lock:
                if self._holder is None or time.time() > self._expires_at:
                    self._holder = holder
                    self._expires_at = time.time() + self.lease_ms / 1000
                    return True
            time.sleep(0.01)
        return False

    def release(self, holder: str) -> bool:
        """释放锁（需验证持有者防止误释放）"""
        with self._lock:
            if self._holder == holder:
                self._holder = None
                return True
            return False

    def renew(self, holder: str) -> bool:
        """续租"""
        with self._lock:
            if self._holder == holder:
                self._expires_at = time.time() + self.lease_ms / 1000
                return True
            return False

    @property
    def holder(self) -> Optional[str]:
        return self._holder


class DistributedCounter:
    """使用分布式锁保护的计数器"""

    def __init__(self):
        self.lock = DistributedLock("counter-lock")
        self.value = 0
        self._lock = threading.Lock()

    def increment(self, holder: str) -> Optional[int]:
        if self.lock.acquire(holder):
            try:
                current = self.value
                time.sleep(0.001)  # 模拟处理延迟
                self.value = current + 1
                return self.value
            finally:
                self.lock.release(holder)
        return None


# ============================================================================
# 2. 乐观锁 — CAS
# ============================================================================

class OptimisticLock:
    """乐观锁 — 版本号机制"""

    def __init__(self):
        self.data = {}
        self.versions = {}

    def read(self, key: str) -> tuple:
        """读取数据 + 版本号"""
        return (self.data.get(key), self.versions.get(key, 0))

    def cas(self, key: str, expected_version: int,
            new_value) -> bool:
        """Compare-And-Swap: 版本号匹配才更新"""
        current_version = self.versions.get(key, 0)
        if current_version != expected_version:
            return False  # 冲突
        self.data[key] = new_value
        self.versions[key] = current_version + 1
        return True

    def update_with_retry(self, key: str, update_fn, max_retries: int = 3) -> bool:
        """带重试的乐观更新"""
        for attempt in range(max_retries):
            val, ver = self.read(key)
            new_val = update_fn(val)
            if self.cas(key, ver, new_val):
                return True
            print(f"  版本冲突，重试 {attempt + 1}/{max_retries}")
        return False


# ============================================================================
# 3. 悲观锁 — 两阶段锁 (2PL)
# ============================================================================

class RowLock:
    """行级锁"""

    def __init__(self, row_id: str):
        self.row_id = row_id
        self.lock = threading.Lock()
        self.holder = None


class TwoPhaseLock:
    """两阶段锁（2PL）调度器"""

    def __init__(self):
        self.row_locks = {}

    def _get_lock(self, row_id: str) -> RowLock:
        if row_id not in self.row_locks:
            self.row_locks[row_id] = RowLock(row_id)
        return self.row_locks[row_id]

    def execute_transaction(self, txn_id: str, operations: list) -> bool:
        """执行两阶段锁事务：先锁后操作，最后释放"""
        locked_rows = []

        # Phase 1: 加锁（扩展阶段）
        print(f"  事务 {txn_id}: 加锁阶段")
        for op in operations:
            row_lock = self._get_lock(op["row_id"])
            acquired = row_lock.lock.acquire(timeout=1)
            if not acquired:
                print(f"    锁定 {op['row_id']} 超时 → 回滚")
                # 释放已获得的锁
                for rl in locked_rows:
                    rl.lock.release()
                return False
            locked_rows.append(row_lock)
            print(f"    锁定 {op['row_id']} (读/写)")

            # 模拟操作
            print(f"    执行: {op['desc']}")

        # Phase 2: 解锁（收缩阶段）
        print(f"  事务 {txn_id}: 解锁阶段")
        for rl in locked_rows:
            rl.lock.release()
            print(f"    解锁 {rl.row_id}")

        return True


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("    分布式同步演示 — Distributed Synchronization")
    print("=" * 56)

    print("\n--- 1. 分布式锁 ---")
    dlock = DistributedLock("resource-1", lease_ms=2000)
    ok = dlock.acquire("instance-A")
    print(f"  instance-A 获取锁: {'[OK]' if ok else '[NO]'}")
    print(f"  当前持有者: {dlock.holder}")

    # 另一个实例尝试获取锁
    ok2 = dlock.acquire("instance-B", timeout_ms=100)
    print(f"  instance-B 获取锁 (超时 100ms): {'[OK]' if ok2 else '[NO]'}")

    # 释放锁
    dlock.release("instance-A")
    print(f"  instance-A 释放锁")
    ok3 = dlock.acquire("instance-B", timeout_ms=100)
    print(f"  instance-B 获取锁: {'[OK]' if ok3 else '[NO]'}")

    print("\n--- 2. 乐观锁 (CAS) ---")
    ol = OptimisticLock()
    ol.data["account:42"] = 100
    ol.versions["account:42"] = 1

    def add_50(val):
        return (val or 0) + 50

    success = ol.update_with_retry("account:42", add_50)
    print(f"  更新账户 42: {'[OK]' if success else '[NO]'}")
    print(f"  当前值: {ol.data['account:42']}, 版本: {ol.versions['account:42']}")

    # 模拟并发冲突
    val, ver = ol.read("account:42")
    ol.cas("account:42", ver - 1, 200)  # 过期版本
    print(f"  过期版本 CAS: 更新失败, 值={ol.data['account:42']}")

    print("\n--- 3. 悲观锁 (两阶段锁) ---")
    tpl = TwoPhaseLock()
    txn_ops = [
        {"row_id": "order-1", "desc": "读取订单金额"},
        {"row_id": "inventory-1", "desc": "扣减库存"},
        {"row_id": "account-1", "desc": "扣减余额"},
    ]
    result = tpl.execute_transaction("T1001", txn_ops)
    print(f"  事务 T1001: {'[OK] 成功' if result else '[NO] 失败'}")

    print("\n--- 4. 锁策略选择指南 ---")
    print(f"  {'场景':<20} {'推荐策略':<16} {'原因':<20}")
    print("-" * 56)
    strategies = [
        ("读多写少", "乐观锁", "冲突少，CAS 高效"),
        ("写多", "悲观锁", "冲突多，CAS 浪费"),
        ("跨进程同步", "分布式锁", "Redis/ZK 协调"),
        ("短操作", "乐观锁", "无锁等待"),
        ("长操作", "悲观锁", "避免反复重试"),
    ]
    for scenario, strategy, reason in strategies:
        print(f"  {scenario:<20} {strategy:<16} {reason:<20}")

    print("\n" + "=" * 56)
    print("提示: 分布式锁需考虑时钟偏差与网络分区")
    print("=" * 56)
