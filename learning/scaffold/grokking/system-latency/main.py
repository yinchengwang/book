"""
延迟分析 — 延迟分解 + P99/P999 计算 + SLA 验证

演示内容:
1. 请求延迟分解: 网络/队列/服务/IO 各阶段耗时
2. P99/P999 计算: 从延迟样本集计算百分位延迟
3. SLA 验证: 判断是否满足 SLA 承诺
"""

import random
from typing import List

random.seed(42)

# ============================================================================
# 1. 延迟分解
# ============================================================================

LATENCY_BREAKDOWN = {
    "DNS 解析": 10,
    "TCP 连接": 20,
    "TLS 握手": 30,
    "请求传输": 5,
    "服务端排队": 15,
    "服务端处理": 50,
    "响应传输": 5,
}


def show_breakdown():
    """打印请求延迟分解（毫秒）"""
    total = sum(LATENCY_BREAKDOWN.values())
    print(f"{'阶段':<12} {'耗时(ms)':<10} {'占比':<8}")
    print("-" * 32)
    for stage, ms in LATENCY_BREAKDOWN.items():
        pct = ms / total * 100
        bar = "#" * int(pct / 5)
        print(f"{stage:<12} {ms:<10} {pct:5.1f}%  {bar}")
    print("-" * 32)
    print(f"{'总计':<12} {total:<10} 100.0%")
    return total


# ============================================================================
# 2. P99/P999 计算
# ============================================================================

def generate_latency_samples(count: int = 10000, base_ms: float = 50,
                             tail_ms: float = 500) -> List[float]:
    """生成模拟延迟样本（正态分布 + 长尾）

    Args:
        count: 样本数
        base_ms: 基准延迟
        tail_ms: 长尾最大延迟

    Returns:
        延迟样本列表（毫秒）
    """
    samples = []
    for _ in range(count):
        if random.random() < 0.1:  # 10% 长尾
            latency = base_ms + random.expovariate(1 / (tail_ms - base_ms))
        else:
            latency = base_ms + random.gauss(0, base_ms * 0.3)
        samples.append(max(0.1, round(latency, 2)))
    return samples


def calc_percentile(sorted_samples: List[float], pct: float) -> float:
    """计算百分位延迟"""
    if not sorted_samples:
        return 0.0
    idx = int(len(sorted_samples) * pct / 100)
    return sorted_samples[min(idx, len(sorted_samples) - 1)]


def show_latency_analysis(samples: List[float]):
    """打印延迟百分位分析"""
    sorted_s = sorted(samples)

    print(f"样本数: {len(samples):,}")
    print(f"平均延迟: {sum(samples) / len(samples):.1f} ms")
    print()

    print(f"{'指标':<8} {'延迟(ms)':<12}")
    print("-" * 22)
    for pct, label in [(50, "P50"), (90, "P90"), (95, "P95"),
                        (99, "P99"), (99.9, "P99.9"), (99.99, "P99.99")]:
        val = calc_percentile(sorted_s, pct)
        flag = " [!]" if val > 200 else ""
        print(f"{label:<8} {val:<12.1f}{flag}")

    # 抖动计算: P99 - P50
    p50 = calc_percentile(sorted_s, 50)
    p99 = calc_percentile(sorted_s, 99)
    print(f"\n抖动 (P99 - P50): {p99 - p50:.1f} ms")


# ============================================================================
# 3. SLA 验证
# ============================================================================

def check_sla(samples: List[float], sla_ms: float = 200,
              sla_pct: float = 99.0) -> dict:
    """验证延迟样本是否满足 SLA

    Args:
        samples: 延迟样本列表
        sla_ms: SLA 延迟阈值
        sla_pct: SLA 百分比

    Returns:
        {"pass": bool, "within_count": int, "total": int, "within_pct": float}
    """
    within = sum(1 for s in samples if s <= sla_ms)
    within_pct = within / len(samples) * 100
    passed = within_pct >= sla_pct
    return {"pass": passed, "within": within, "total": len(samples),
            "within_pct": round(within_pct, 2)}


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("        延迟分析演示 — Latency Analysis")
    print("=" * 56)

    print("\n--- 1. 请求延迟分解 ---")
    total_ms = show_breakdown()
    print(f"\n一次请求端到端延迟: ~{total_ms} ms")

    print("\n--- 2. 延迟百分位分析 ---")
    samples = generate_latency_samples(10000)
    show_latency_analysis(samples)

    print("\n--- 3. SLA 验证 ---")
    sla = check_sla(samples, sla_ms=200, sla_pct=99.0)
    status = "[OK] 通过" if sla["pass"] else "[NO] 未通过"
    print(f"SLA: ≤200 ms @ 99.0% → {status}")
    print(f"  {sla['within']:,}/{sla['total']:,} = {sla['within_pct']}%")

    # 更严格 SLA
    sla_strict = check_sla(samples, sla_ms=100, sla_pct=99.9)
    status_s = "[OK] 通过" if sla_strict["pass"] else "[NO] 未通过"
    print(f"SLA: ≤100 ms @ 99.9% → {status_s}")
    print(f"  {sla_strict['within']:,}/{sla_strict['total']:,}"
          f" = {sla_strict['within_pct']}%")

    print("\n" + "=" * 56)
    print("提示: P99/P999 是衡量用户体验的核心指标")
    print("=" * 56)
