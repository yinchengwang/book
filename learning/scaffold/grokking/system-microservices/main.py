"""
微服务架构 — 服务拆分 + API 网关 + 服务发现

演示内容:
1. 服务拆分: 按业务领域拆分为独立服务
2. API 网关: 请求路由/聚合/限流
3. 服务发现: 注册中心 + 健康检查
"""

import time
from typing import Optional

# ============================================================================
# 1. 微服务定义
# ============================================================================

class MicroService:
    """单个微服务"""

    def __init__(self, name: str, version: str = "1.0.0",
                 instances: int = 2):
        self.name = name
        self.version = version
        self.instances = [f"{name}-{i}" for i in range(instances)]

    def handle(self, request: str) -> dict:
        """处理请求（模拟）"""
        return {
            "service": self.name,
            "instance": self.instances[0],
            "result": f"[{self.name}] processed: {request}",
        }


class Monolith:
    """单体架构对比"""

    def __init__(self):
        self.modules = ["user", "order", "payment", "inventory", "notification"]

    def handle(self, request: str) -> dict:
        return {
            "service": "monolith",
            "modules": self.modules,
            "result": f"[monolith] all modules in one process: {request}",
        }


# ============================================================================
# 2. API 网关
# ============================================================================

class APIGateway:
    """API 网关：路由 + 聚合 + 限流"""

    def __init__(self):
        self.routes = {}
        self.rate_limits = {}  # service → (max_rpm, current_count)
        self.request_count = {}

    def register_route(self, path: str, service: MicroService) -> None:
        self.routes[path] = service

    def set_rate_limit(self, service: str, max_rpm: int) -> None:
        self.rate_limits[service] = max_rpm
        self.request_count[service] = 0

    def route(self, path: str, request: str) -> Optional[dict]:
        """路由请求到对应微服务"""
        # 1. 路径匹配
        service = None
        for prefix, svc in self.routes.items():
            if path.startswith(prefix):
                service = svc
                break
        if not service:
            return {"error": f"No route for {path}"}

        # 2. 限流检查
        svc_name = service.name
        if svc_name in self.rate_limits:
            self.request_count[svc_name] += 1
            if self.request_count[svc_name] > self.rate_limits[svc_name]:
                return {"error": f"Rate limit exceeded for {svc_name}"}

        # 3. 转发
        return service.handle(request)

    def aggregate(self, requests: list) -> list:
        """聚合多个请求结果"""
        results = []
        for path, req in requests:
            results.append(self.route(path, req))
        return results


# ============================================================================
# 3. 服务发现
# ============================================================================

class ServiceRegistry:
    """服务注册中心"""

    def __init__(self):
        self.services = {}  # name → {instances, health}

    def register(self, name: str, instance: str) -> None:
        if name not in self.services:
            self.services[name] = {"instances": [], "health": {}}
        self.services[name]["instances"].append(instance)
        self.services[name]["health"][instance] = True

    def unregister(self, name: str, instance: str) -> None:
        if name in self.services:
            svc = self.services[name]
            if instance in svc["instances"]:
                svc["instances"].remove(instance)
            svc["health"].pop(instance, None)

    def health_check(self, name: str, instance: str,
                     healthy: bool) -> None:
        """更新健康状态"""
        if name in self.services:
            self.services[name]["health"][instance] = healthy

    def discover(self, name: str) -> list:
        """发现服务的健康实例"""
        svc = self.services.get(name)
        if not svc:
            return []
        return [inst for inst in svc["instances"]
                if svc["health"].get(inst, False)]

    def status(self) -> dict:
        result = {}
        for name, svc in self.services.items():
            healthy = sum(1 for h in svc["health"].values() if h)
            total = len(svc["instances"])
            result[name] = f"{healthy}/{total} healthy"
        return result


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("    微服务架构演示 — Microservices")
    print("=" * 56)

    print("\n--- 1. 服务拆分: 单体 vs 微服务 ---")
    monolith = Monolith()
    result = monolith.handle("create_order")
    print(f"  单体: {result['result']}")
    print(f"  所有模块: {result['modules']}")

    services = {
        "user": MicroService("user-service", instances=3),
        "order": MicroService("order-service", instances=2),
        "payment": MicroService("payment-service", instances=2),
        "inventory": MicroService("inventory-service", instances=2),
        "notification": MicroService("notification-service", instances=1),
    }
    print(f"  微服务: {len(services)} 个独立服务, "
          f"共 {sum(len(s.instances) for s in services.values())} 个实例")

    print("\n--- 2. API 网关 ---")
    gateway = APIGateway()
    for prefix, svc in services.items():
        gateway.register_route(f"/{prefix}", svc)

    # 路由请求
    test_requests = [
        ("/user/profile", "get user info"),
        ("/order/create", "create order"),
        ("/payment/pay", "pay order"),
        ("/unknown/route", "test"),
    ]
    for path, req in test_requests:
        result = gateway.route(path, req)
        if "error" in result:
            print(f"  {path} → [NO] {result['error']}")
        else:
            print(f"  {path} → [OK] {result['service']}")

    print("\n--- 3. 服务发现 ---")
    registry = ServiceRegistry()
    registry.register("user-service", "user-svc-1")
    registry.register("user-service", "user-svc-2")
    registry.register("user-service", "user-svc-3")
    registry.register("order-service", "order-svc-1")
    registry.register("order-service", "order-svc-2")

    # 模拟健康检查
    registry.health_check("user-service", "user-svc-2", False)
    registry.health_check("order-service", "order-svc-1", True)

    print(f"  注册状态:")
    for svc, status in registry.status().items():
        print(f"    {svc}: {status}")

    print(f"  用户服务可用实例: {registry.discover('user-service')}")
    print(f"  订单服务可用实例: {registry.discover('order-service')}")

    print("\n" + "=" * 56)
    print("提示: 微服务以分布式复杂性换取独立部署能力")
    print("=" * 56)
