"""
CDN 设计 — 内容分发 + 缓存策略 + 动态加速

演示内容:
1. CDN 节点架构: 边缘节点/中间源/源站
2. 缓存策略: TTL/强制刷新/预热
3. 动态加速: DSA 原理 + 最佳路径选择
"""

# ============================================================================
# 1. CDN 节点架构
# ============================================================================

class CDNNode:
    """CDN 节点模拟"""

    def __init__(self, name: str, region: str, tier: str = "edge"):
        self.name = name
        self.region = region
        self.tier = tier
        self.cache = {}

    def serve(self, path: str) -> dict:
        """尝试从本地缓存响应"""
        if path in self.cache:
            return {"data": self.cache[path], "source": f"{self.name}(cache)"}
        return {"data": None, "source": self.name}

    def store(self, path: str, data) -> None:
        self.cache[path] = data


class CDNArchitecture:
    """演示 CDN 三层架构"""

    def __init__(self):
        self.edges = [
            CDNNode("边缘-北京", "华北"),
            CDNNode("边缘-上海", "华东"),
            CDNNode("边缘-广州", "华南"),
            CDNNode("边缘-成都", "西部"),
        ]
        self.intermediate = CDNNode("中间源-武汉", "华中", "intermediate")
        self.origin = CDNNode("源站-北京", "华北", "origin")

    def request(self, path: str, user_region: str) -> dict:
        """用户请求经过 CDN 的完整路径"""
        # 1. 找最近边缘节点
        region_map = {"华北": "边缘-北京", "华东": "边缘-上海",
                       "华南": "边缘-广州", "西部": "边缘-成都", "华中": "边缘-成都"}
        edge_name = region_map.get(user_region, "边缘-北京")
        edge = next(e for e in self.edges if e.name == edge_name)

        # 2. 边缘节点检查缓存
        result = edge.serve(path)
        if result["data"] is not None:
            return {"data": result["data"],
                    "path": f"用户 → {edge.name} (缓存命中)",
                    "latency_ms": 5}

        # 3. 边缘未命中 → 查中间源
        result = self.intermediate.serve(path)
        if result["data"] is not None:
            data = result["data"]
            edge.store(path, data)
            return {"data": data,
                    "path": f"用户 → {edge.name} → {self.intermediate.name}",
                    "latency_ms": 20}

        # 4. 中间源未命中 → 回源站
        data = self.origin.serve(path)["data"]
        # 同步回填
        self.intermediate.store(path, data)
        edge.store(path, data)
        return {"data": data,
                "path": f"用户 → {edge.name} → {self.intermediate.name} → {self.origin.name}",
                "latency_ms": 100}


# ============================================================================
# 2. 缓存策略
# ============================================================================

class CacheStrategy:
    """演示不同缓存策略"""

    @staticmethod
    def ttl_based(content_type: str) -> int:
        """按内容类型推荐 TTL（秒）"""
        ttl_map = {
            "image/png": 604800,       # 7 天
            "image/jpeg": 604800,      # 7 天
            "text/css": 86400,         # 1 天
            "application/javascript": 86400,  # 1 天
            "text/html": 300,          # 5 分钟
            "application/json": 60,    # 1 分钟
            "text/plain": 0,           # 不缓存
        }
        return ttl_map.get(content_type, 0)

    @staticmethod
    def cache_control_header(ttl: int) -> str:
        if ttl <= 0:
            return "Cache-Control: no-cache, no-store"
        if ttl <= 60:
            return f"Cache-Control: public, max-age={ttl}"
        if ttl <= 3600:
            return f"Cache-Control: public, max-age={ttl}, must-revalidate"
        return f"Cache-Control: public, max-age={ttl}, immutable"


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("        CDN 设计演示 — Content Delivery Network")
    print("=" * 56)

    print("\n--- 1. CDN 三层架构 ---")
    # 先在源站放数据
    arch = CDNArchitecture()
    arch.origin.cache["/static/bg.png"] = "image-bytes"

    regions = ["华北", "华东", "华南", "西部"]
    for region in regions:
        result = arch.request("/static/bg.png", region)
        print(f"  {region}用户 → {result['path']} ({result['latency_ms']}ms)")

    # 第二次请求（缓存命中）
    print(f"\n  --- 第二次请求（缓存命中） ---")
    result = arch.request("/static/bg.png", "华北")
    print(f"  华北用户 → {result['path']} ({result['latency_ms']}ms)")

    print("\n--- 2. 缓存策略对比 ---")
    content_types = [
        "image/png", "text/css", "application/javascript",
        "text/html", "application/json", "text/plain",
    ]
    print(f"{'内容类型':<28} {'TTL':<12} {'Cache-Control':<45}")
    print("-" * 85)
    for ct in content_types:
        ttl = CacheStrategy.ttl_based(ct)
        cc = CacheStrategy.cache_control_header(ttl)
        print(f"{ct:<28} {ttl:<12} {cc:<45}")

    print("\n--- 3. CDN 关键指标 ---")
    metrics = {
        "缓存命中率": "95%+",
        "边缘节点数": "全球 2800+ 节点（以 CloudFlare/Akamai 为参考）",
        "动态加速": "实时探测最优路径，避开拥堵链路",
        "HTTPS 卸载": "边缘节点 SSL 终结，源站负载降低",
        "DDoS 防护": "清洗恶意流量，保护源站",
    }
    for k, v in metrics.items():
        print(f"  {k}: {v}")

    print("\n" + "=" * 56)
    print("提示: CDN 是大型系统的第一道防线")
    print("=" * 56)
