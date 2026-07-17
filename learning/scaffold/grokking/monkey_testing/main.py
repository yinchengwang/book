#!/usr/bin/env python3
"""
混沌测试/故障注入 演示

模拟一个微服务调用链，注入各种故障（网络延迟、服务崩溃、资源耗尽），
观察熔断器和重试退避策略如何保护系统。
"""

import time
import random
import threading
from enum import Enum
from typing import Optional, Callable


class ServiceState(Enum):
    """服务状态"""
    HEALTHY = "healthy"
    DEGRADED = "degraded"
    DOWN = "down"


class CircuitBreakerState(Enum):
    """熔断器状态"""
    CLOSED = "closed"       # 正常, 请求通过
    OPEN = "open"           # 熔断, 快速失败
    HALF_OPEN = "half_open"  # 半开, 试探恢复


class CircuitBreaker:
    """
    熔断器实现

    三态: closed (正常) → open (熔断) → half_open (半开) → closed
    """

    def __init__(self, failure_threshold: int = 3,
                 recovery_timeout: float = 2.0,
                 half_open_max_requests: int = 2):
        self.state = CircuitBreakerState.CLOSED
        self.failure_count = 0
        self.failure_threshold = failure_threshold
        self.recovery_timeout = recovery_timeout
        self.last_failure_time = 0.0
        self.half_open_max_requests = half_open_max_requests
        self.half_open_requests = 0
        self._lock = threading.Lock()

    def call(self, func: Callable, *args, **kwargs):
        """执行调用, 熔断器保护"""
        with self._lock:
            if self.state == CircuitBreakerState.OPEN:
                if time.time() - self.last_failure_time >= self.recovery_timeout:
                    print("[熔断器] 超时已到, 进入半开状态")
                    self.state = CircuitBreakerState.HALF_OPEN
                    self.half_open_requests = 0
                else:
                    raise RuntimeError("熔断器打开, 请求被拒绝")

            if self.state == CircuitBreakerState.HALF_OPEN:
                if self.half_open_requests >= self.half_open_max_requests:
                    raise RuntimeError("半开状态限流, 请求被拒绝")
                self.half_open_requests += 1

        try:
            result = func(*args, **kwargs)
            with self._lock:
                if self.state == CircuitBreakerState.HALF_OPEN:
                    print("[熔断器] 半开状态调用成功, 恢复为关闭")
                    self.state = CircuitBreakerState.CLOSED
                    self.failure_count = 0
                elif self.state == CircuitBreakerState.CLOSED:
                    self.failure_count = 0
            return result
        except Exception as e:
            with self._lock:
                self.failure_count += 1
                self.last_failure_time = time.time()
                if self.failure_count >= self.failure_threshold:
                    print(f"[熔断器] 失败 {self.failure_count} 次, 打开熔断器")
                    self.state = CircuitBreakerState.OPEN
            raise e


def retry_with_backoff(max_retries: int = 3,
                       base_delay: float = 0.1,
                       max_delay: float = 2.0,
                       jitter: bool = True):
    """
    指数退避重试装饰器

    延迟 = min(base_delay * 2^attempt, max_delay)
    可选随机抖动避免重试风暴
    """
    def decorator(func):
        def wrapper(*args, **kwargs):
            last_exception = None
            for attempt in range(max_retries + 1):
                try:
                    return func(*args, **kwargs)
                except Exception as e:
                    last_exception = e
                    if attempt < max_retries:
                        delay = min(base_delay * (2 ** attempt), max_delay)
                        if jitter:
                            delay = delay * (0.5 + random.random() * 0.5)
                        print(f"[重试] 第{attempt + 1}次失败, "
                              f"{delay:.2f}s 后重试 ({e})")
                        time.sleep(delay)
                    else:
                        print(f"[重试] 已达最大重试次数 {max_retries}")
            raise last_exception
        return wrapper
    return decorator


# ============================================================
# 模拟服务
# ============================================================

class SimulationService:
    """模拟的外部服务, 支持故障注入"""

    def __init__(self, name: str, base_latency: float = 0.05):
        self.name = name
        self.base_latency = base_latency
        self.state = ServiceState.HEALTHY
        self.failure_rate = 0.0        # 故障概率 0.0~1.0
        self.extra_latency = 0.0       # 额外延迟(秒)
        self.failure_count = 0
        self.success_count = 0

    def inject_failure(self, failure_rate: float = 0.0,
                       extra_latency: float = 0.0,
                       state: ServiceState = ServiceState.HEALTHY):
        """注入故障"""
        self.failure_rate = failure_rate
        self.extra_latency = extra_latency
        self.state = state

    def call(self) -> str:
        """模拟一次服务调用"""
        time.sleep(self.base_latency)

        if self.extra_latency > 0:
            time.sleep(self.extra_latency)

        if self.state == ServiceState.DOWN:
            self.failure_count += 1
            raise RuntimeError(f"{self.name} 服务已宕机")

        if random.random() < self.failure_rate:
            self.failure_count += 1
            raise RuntimeError(f"{self.name} 服务调用失败 (故障注入)")

        self.success_count += 1
        return f"{self.name} 响应成功"


class ChaosExperiment:
    """
    混沌实验控制器

    模拟 Netflix Chaos Monkey 思路, 在服务调用链中注入故障
    """

    def __init__(self):
        self.services = {}
        self.chain_order = []
        self.circuit_breakers = {}

    def add_service(self, name: str, base_latency: float = 0.05):
        svc = SimulationService(name, base_latency)
        self.services[name] = svc
        self.chain_order.append(name)
        self.circuit_breakers[name] = CircuitBreaker(
            failure_threshold=2, recovery_timeout=1.0)
        return svc

    @retry_with_backoff(max_retries=2, base_delay=0.1)
    def _call_with_retry(self, service_name: str) -> str:
        """带重试的服务调用"""
        cb = self.circuit_breakers[service_name]
        return cb.call(self.services[service_name].call)

    def execute_chain(self) -> list:
        """执行完整的服务调用链"""
        results = []
        for name in self.chain_order:
            try:
                result = self._call_with_retry(name)
                results.append((name, "success", result))
            except Exception as e:
                results.append((name, "failure", str(e)))
        return results

    def run_scenario(self, scenario_name: str,
                     injections: dict,
                     iterations: int = 3) -> dict:
        """
        运行一个混沌实验场景

        injections: {service_name: {failure_rate, extra_latency, state}}
        """
        print(f"\n{'='*60}")
        print(f"[实验] {scenario_name}")
        print(f"{'='*60}")

        # 注入故障
        for svc_name, params in injections.items():
            if svc_name in self.services:
                self.services[svc_name].inject_failure(**params)
                print(f"  -> {svc_name}: {params}")

        # 清除统计
        for svc in self.services.values():
            svc.failure_count = 0
            svc.success_count = 0

        # 执行实验
        iteration_results = []
        for i in range(iterations):
            time.sleep(0.1)
            result = self.execute_chain()
            iteration_results.append(result)
            status = "/".join(r[1] for r in result)
            print(f"  [{i + 1}] {status}")

        return {"scenario": scenario_name, "results": iteration_results}


def main():
    print("=" * 60)
    print("混沌测试/故障注入 演示")
    print("模拟服务调用链 + 熔断器 + 重试退避")
    print("=" * 60)

    experiment = ChaosExperiment()
    experiment.add_service("api-gateway", 0.02)
    experiment.add_service("user-service", 0.03)
    experiment.add_service("order-service", 0.05)
    experiment.add_service("payment-service", 0.08)
    experiment.add_service("inventory-service", 0.04)

    print(f"\n服务调用链: {' → '.join(experiment.chain_order)}")
    print(f"熔断器: 失败阈值=2, 恢复超时=1.0s, 半开最大请求=1")

    # 场景 1: 正常状态
    experiment.run_scenario("场景1: 所有服务正常", injections={})

    # 场景 2: 网络延迟
    experiment.run_scenario(
        "场景2: payment-service 高延迟",
        injections={
            "payment-service": {
                "failure_rate": 0.0,
                "extra_latency": 0.7,
                "state": ServiceState.HEALTHY
            }
        }
    )

    # 场景 3: 故障注入 + 熔断
    experiment.run_scenario(
        "场景3: order-service 故障注入 (触发熔断)",
        injections={
            "order-service": {
                "failure_rate": 0.8,
                "extra_latency": 0.0,
                "state": ServiceState.HEALTHY
            }
        },
        iterations=5
    )

    # 场景 4: 服务完全宕机
    experiment.run_scenario(
        "场景4: payment-service 完全宕机",
        injections={
            "payment-service": {
                "failure_rate": 0.0,
                "extra_latency": 0.0,
                "state": ServiceState.DOWN
            }
        },
        iterations=3
    )

    # 场景 5: 级联故障 (多个服务同时故障)
    experiment.run_scenario(
        "场景5: 级联故障 (api-gateway + user-service 同时故障)",
        injections={
            "api-gateway": {
                "failure_rate": 0.6,
                "extra_latency": 0.0,
                "state": ServiceState.HEALTHY
            },
            "user-service": {
                "failure_rate": 0.0,
                "extra_latency": 0.5,
                "state": ServiceState.HEALTHY
            }
        },
        iterations=4
    )

    print(f"\n{'='*60}")
    print("实验总结")
    print(f"{'='*60}")
    for name, svc in experiment.services.items():
        print(f"  {name}: 成功={svc.success_count}, 失败={svc.failure_count}")

    print("\n演示完成。")


if __name__ == "__main__":
    main()
