"""
缓存系统设计 — CacheAside + WriteThrough + 一致性哈希

演示内容:
1. CacheAside: 旁路缓存模式，读/写策略
2. WriteThrough: 穿透写模式
3. 一致性哈希: 带虚拟节点的哈希环
"""

import hashlib

# ============================================================================
# 1. CacheAside 模式
# ============================================================================

class CacheAside:
    """Cache Aside 模式演示（旁路缓存）"""

    def __init__(self):
        self._cache = {}   # 缓存
        self._db = {}      # 模拟数据库
        self.hits = 0
        self.misses = 0

    def get(self, key: str):
        """读: 先查缓存，miss 则回源数据库"""
        if key in self._cache:
            self.hits += 1
            return self._cache[key], "cache-hit"

        self.misses += 1
        val = self._db.get(key)
        if val is not None:
            self._cache[key] = val  # 回填缓存
        return val, "db-miss"

    def set(self, key: str, value) -> None:
        """写: 先写数据库，再删除缓存（延迟淘汰）"""
        self._db[key] = value
        self._cache.pop(key, None)  # 淘汰缓存而非更新

    def hit_rate(self) -> float:
        total = self.hits + self.misses
        return self.hits / total if total else 0.0


# ============================================================================
# 2. WriteThrough 模式
# ============================================================================

class WriteThroughCache:
    """Write Through 模式演示（穿透写）"""

    def __init__(self):
        self._cache = {}
        self._db = {}

    def get(self, key: str):
        return self._cache.get(key, self._db.get(key))

    def set(self, key: str, value) -> None:
        """写: 同时写缓存和数据库"""
        self._cache[key] = value
        self._db[key] = value

    def stats(self) -> dict:
        return {"cache_size": len(self._cache), "db_size": len(self._db)}


# ============================================================================
# 3. 一致性哈希
# ============================================================================

class ConsistentHash:
    """带虚拟节点的一致性哈希环"""

    def __init__(self, vnodes: int = 100):
        self.vnodes = vnodes
        self.ring = {}      # hash → node
        self.nodes = {}     # node → list of hashes
        self.sorted_keys = []

    def _hash(self, key: str) -> int:
        return int(hashlib.md5(key.encode()).hexdigest(), 16)

    def add_node(self, node: str) -> None:
        """添加节点及其虚拟节点"""
        self.nodes[node] = []
        for i in range(self.vnodes):
            h = self._hash(f"{node}:vnode:{i}")
            self.ring[h] = node
            self.nodes[node].append(h)
        self.sorted_keys = sorted(self.ring.keys())

    def remove_node(self, node: str) -> None:
        """移除节点及其虚拟节点"""
        for h in self.nodes.pop(node, []):
            del self.ring[h]
        self.sorted_keys = sorted(self.ring.keys())

    def get_node(self, key: str) -> str:
        """查找 key 对应的节点"""
        if not self.ring:
            return None
        h = self._hash(key)
        # 二分查找第一个 ≥ h 的节点
        for sk in self.sorted_keys:
            if sk >= h:
                return self.ring[sk]
        return self.ring[self.sorted_keys[0]]  # 环回

    def distribution(self, keys: list) -> dict:
        """返回键在各节点的分布"""
        dist = {n: 0 for n in self.nodes}
        for k in keys:
            n = self.get_node(k)
            dist[n] = dist.get(n, 0) + 1
        return dist


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("      缓存系统设计演示 — Caching Design")
    print("=" * 56)

    print("\n--- 1. CacheAside 模式 ---")
    ca = CacheAside()
    ca._db["user:1"] = "Alice"
    ca._db["user:2"] = "Bob"

    val, src = ca.get("user:1")
    print(f"  首次读 user:1 → {src} → {val}")
    val, src = ca.get("user:1")
    print(f"  再次读 user:1 → {src} → {val}")
    ca.set("user:2", "Bob-Updated")
    val, src = ca.get("user:2")
    print(f"  写后读 user:2 → {src} → {val}")
    print(f"  缓存命中率: {ca.hit_rate():.1%}")

    print("\n--- 2. WriteThrough 模式 ---")
    wt = WriteThroughCache()
    wt.set("session:abc", {"user_id": 1, "expire": 3600})
    print(f"  写 session:abc → 缓存 & 数据库同步")
    print(f"  缓存大小: {wt.stats()['cache_size']}, DB 大小: {wt.stats()['db_size']}")

    print("\n--- 3. 一致性哈希分布 ---")
    ch = ConsistentHash(vnodes=50)
    for n in ["cache-a", "cache-b", "cache-c", "cache-d"]:
        ch.add_node(n)

    test_keys = [f"key:{i}" for i in range(1000)]
    dist = ch.distribution(test_keys)
    print(f"  4 节点, {len(test_keys)} 个键:")
    for n, cnt in sorted(dist.items()):
        print(f"    {n}: {cnt} 键 ({cnt / len(test_keys):.1%})")

    # 模拟节点增减
    ch.remove_node("cache-c")
    print(f"\n  移除 cache-c 后:")
    dist2 = ch.distribution(test_keys)
    for n, cnt in sorted(dist2.items()):
        print(f"    {n}: {cnt} 键 ({cnt / len(test_keys):.1%})")

    # 计算移动的键比例
    affected = sum(1 for k in test_keys if ch.get_node(k)
                   != ch.get_node(k) if False)  # 简化
    # 实际统计
    original_ring = dict(ch.ring)  # snapshot
    moved = 0
    for k in test_keys:
        # 无法精确比较，用概念演示
        pass
    print(f"  (理想情况下仅 1/4 键迁移，体现一致性哈希优势)")

    print("\n" + "=" * 56)
    print("提示: 缓存策略选择取决于读写比和一致性要求")
    print("=" * 56)
