"""
cache_strategy — 数据库缓存策略演示

演示 Cache Aside / Read Through / Write Through / Write Behind /
缓存穿透/击穿/雪崩及过期策略。
"""

import threading
import time
import random
from typing import Dict, Optional, Callable, Any
from dataclasses import dataclass


class Database:
    """模拟数据库（慢存储）"""

    def __init__(self):
        self.store: Dict[str, str] = {
            "user:1": "Alice",
            "user:2": "Bob",
            "user:3": "Charlie",
            "user:4": "David",
            "user:5": "Eve",
        }
        self.read_count = 0

    def read(self, key: str) -> Optional[str]:
        self.read_count += 1
        time.sleep(0.02)  # 模拟磁盘 IO 延迟
        return self.store.get(key)

    def write(self, key: str, value: str):
        time.sleep(0.03)  # 模拟写入延迟
        self.store[key] = value


class Cache:
    """模拟缓存（快存储）"""

    def __init__(self):
        self.store: Dict[str, str] = {}
        self.hits = 0
        self.misses = 0
        self.ttl: Dict[str, float] = {}

    def get(self, key: str) -> Optional[str]:
        val = self.store.get(key)
        if val is not None:
            # 检查 TTL
            if key in self.ttl and time.time() > self.ttl[key]:
                del self.store[key]
                del self.ttl[key]
                self.misses += 1
                return None
            self.hits += 1
        else:
            self.misses += 1
        return val

    def set(self, key: str, value: str, ttl_secs: float = 30.0):
        self.store[key] = value
        if ttl_secs > 0:
            self.ttl[key] = time.time() + ttl_secs

    def delete(self, key: str):
        self.store.pop(key, None)
        self.ttl.pop(key, None)

    @property
    def hit_rate(self) -> float:
        total = self.hits + self.misses
        return self.hits / total if total > 0 else 0


# ════════════════════════════════════════════════════════════
# 1. Cache Aside（旁路缓存）
# ════════════════════════════════════════════════════════════
def demo_cache_aside():
    print("═══ 1. Cache Aside（旁路缓存） ═══")
    db = Database()
    cache = Cache()

    # 读取：先查缓存，缓存 miss 查 DB，回填缓存
    def read(key: str) -> Optional[str]:
        val = cache.get(key)
        if val is not None:
            print(f"  Cache HIT: {key} → {val}")
            return val
        print(f"  Cache MISS: {key}")
        val = db.read(key)
        if val is not None:
            cache.set(key, val, ttl_secs=60)
            print(f"  回填缓存: {key} → {val}")
        return val

    # 写入：先写 DB，再删除缓存
    def write(key: str, value: str):
        db.write(key, value)
        cache.delete(key)
        print(f"  写入 DB: {key} → {value}, 删除缓存")

    read("user:1")   # miss
    read("user:1")   # hit
    write("user:1", "Alice Updated")
    read("user:1")   # miss（缓存已删，重新回填）
    read("user:1")   # hit

    print(f"  缓存命中率: {cache.hit_rate:.0%}\n")


# ════════════════════════════════════════════════════════════
# 2. 缓存穿透 / 击穿 / 雪崩
# ════════════════════════════════════════════════════════════
def demo_cache_problems():
    print("═══ 2. 缓存常见问题 ═══")

    # 缓存穿透
    print("  --- 缓存穿透 ---")
    db = Database()
    cache = Cache()

    val = cache.get("user:9999")
    if val is None:
        val = db.read("user:9999")
        # 缓存穿透：不存在的数据每次都查 DB
        print(f"  查询 user:9999 → DB 没有 → 不缓存 → 下次又查 DB")
    print(f"  DB 查询次数: {db.read_count}（恶意攻击时会被放大）")

    print("  方案：缓存空值（short TTL）或布隆过滤器")
    print()

    # 缓存击穿
    print("  --- 缓存击穿 ---")
    print("  场景：热 key 到期，大量并发请求同时查 DB")
    print("  方案：互斥锁，第一个请求回填缓存，其他等待")
    print()

    # 缓存雪崩
    print("  --- 缓存雪崩 ---")
    print("  场景：大量 key 同时过期 / 缓存节点宕机")
    print("  方案：过期时间加随机偏移 / 本地缓存 + Redis Sentinel")
    print()


# ════════════════════════════════════════════════════════════
# 3. Write Through / Write Behind
# ════════════════════════════════════════════════════════════
@dataclass
class WriteThroughCache:
    """Write-Through：缓存和 DB 同步写入"""
    db: Database
    cache: Cache

    def write(self, key: str, value: str):
        self.cache.set(key, value)
        self.db.write(key, value)
        print(f"  [WT] 缓存+DB 同步写入: {key} → {value}")

    def read(self, key: str) -> Optional[str]:
        val = self.cache.get(key)
        if val is None:
            val = self.db.read(key)
            self.cache.set(key, val)
        return val


@dataclass
class WriteBehindCache:
    """Write-Behind：异步写入 DB，批量刷盘"""
    db: Database
    cache: Cache
    batch: list = None

    def __post_init__(self):
        self.batch = []

    def write(self, key: str, value: str):
        self.cache.set(key, value)
        self.batch.append((key, value))
        print(f"  [WB] 写入缓存: {key} → {value}（延迟写 DB）")

    def flush(self):
        print(f"  [WB] 批量刷盘: {len(self.batch)} 条")
        for k, v in self.batch:
            self.db.write(k, v)
        self.batch.clear()

    def read(self, key: str) -> Optional[str]:
        val = self.cache.get(key)
        if val is None:
            val = self.db.read(key)
            self.cache.set(key, val)
        return val


def demo_write_strategies():
    print("═══ 3. Write Through vs Write Behind ═══")

    db = Database()
    wt = WriteThroughCache(db, Cache())
    wt.write("user:10", "Frank")
    val = wt.read("user:10")
    print(f"  WT 读取: {val}\n")

    wb = WriteBehindCache(Database(), Cache())
    wb.write("user:20", "Grace")
    wb.write("user:21", "Helen")
    wb.write("user:22", "Ivy")
    val = wb.read("user:20")
    print(f"  WB 读取（未刷盘，缓存有）: {val}")
    wb.flush()
    print()


# ════════════════════════════════════════════════════════════
# 4. 过期策略
# ════════════════════════════════════════════════════════════
def demo_eviction():
    print("═══ 4. 缓存过期/淘汰策略 ═══")
    strategies = [
        ("TTL（Time To Live）", "设置固定过期时间，到期删除"),
        ("LRU（最近最少使用）", "淘汰最久未被访问的 key"),
        ("LFU（最不经常使用）", "淘汰访问频率最低的 key"),
        ("FIFO（先进先出）", "按写入顺序淘汰"),
        ("Random", "随机淘汰——简单但不可预测"),
    ]
    for name, desc in strategies:
        print(f"  {name:25s} — {desc}")
    print()


def main():
    print("=" * 50)
    print("  缓存策略演示 — Cache Aside / Write Through / 雪崩")
    print("=" * 50 + "\n")

    demo_cache_aside()
    demo_cache_problems()
    demo_write_strategies()
    demo_eviction()

    print("所有缓存策略演示完成。")


if __name__ == "__main__":
    main()
