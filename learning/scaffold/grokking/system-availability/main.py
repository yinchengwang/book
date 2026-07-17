"""
高可用设计 — 可用性计算 + MTTF/MTTR + 冗余策略

演示内容:
1. 可用性计算: 9s 表示法与年停机时间换算
2. MTTF/MTTR: 平均无故障时间 / 平均修复时间
3. 冗余策略: 单机/主备/双活/多活对比
"""

# ============================================================================
# 1. 可用性计算
# ============================================================================

AVAILABILITY_9S = {
    "90% (1个9)":      (0.9, 876 * 60),        # 876 小时/年
    "99% (2个9)":      (0.99, 87.6 * 60),      # 87.6 小时/年
    "99.9% (3个9)":    (0.999, 8.76 * 60),     # 8.76 小时/年
    "99.99% (4个9)":   (0.9999, 52.56),        # 52.56 分钟/年
    "99.999% (5个9)":  (0.99999, 5.26),        # 5.26 分钟/年
    "99.9999% (6个9)": (0.999999, 31.54),      # 31.54 秒/年
}


def calc_availability(mttf_hours: float, mttr_hours: float) -> dict:
    """按 MTTF/MTTR 计算可用性

    Args:
        mttf_hours: 平均无故障时间（小时）
        mttr_hours: 平均修复时间（小时）

    Returns:
        {"availability": float, "downtime_minutes_year": float}
    """
    avail = mttf_hours / (mttf_hours + mttr_hours)
    downtime_year = (1 - avail) * 365 * 24 * 60  # 分钟/年
    return {
        "availability": avail,
        "availability_pct": round(avail * 100, 4),
        "downtime_year_min": round(downtime_year, 2),
    }


# ============================================================================
# 2. 冗余策略对比
# ============================================================================

def redundancy_strategies():
    """返回各冗余策略的可用性模型"""
    return [
        {
            "name": "单机无冗余",
            "desc": "单实例运行，无备份",
            "availability_pct": 99.0,
            "cost": "低",
            "complexity": "低",
        },
        {
            "name": "主备 (Active-Standby)",
            "desc": "一台主用、一台备用，故障时切换",
            "availability_pct": 99.9,
            "cost": "中",
            "complexity": "中",
        },
        {
            "name": "双活 (Active-Active)",
            "desc": "两台同时服务，故障时流量切换",
            "availability_pct": 99.99,
            "cost": "中高",
            "complexity": "中高",
        },
        {
            "name": "多活 (Multi-Region)",
            "desc": "多区域同时服务，区域级故障冗余",
            "availability_pct": 99.999,
            "cost": "高",
            "complexity": "高",
        },
    ]


# ============================================================================
# 3. 级联可用性计算
# ============================================================================

def calc_composite_availability(components: list) -> float:
    """计算串联系统整体可用性（乘积法则）"""
    result = 1.0
    for name, avail in components:
        result *= avail
    return result


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("      高可用设计演示 — Availability Design")
    print("=" * 56)

    print("\n--- 1. 可用性级别与年停机时间 ---")
    print(f"{'级别':<18} {'可用性':<10} {'年停机时间':<15}")
    print("-" * 45)
    for label, (avail, downtime_min) in sorted(
            AVAILABILITY_9S.items(), key=lambda x: x[1][0]):
        if downtime_min >= 60:
            d_str = f"{downtime_min / 60:.1f} 小时"
        elif downtime_min >= 1:
            d_str = f"{downtime_min:.1f} 分钟"
        else:
            d_str = f"{downtime_min:.1f} 秒"
        print(f"{label:<18} {avail:<10.4%} {d_str:<15}")

    print("\n--- 2. MTTF/MTTR 计算 ---")
    scenarios = [
        ("普通服务器", 720, 4),   # MTTF=30天, MTTR=4小时
        ("高可靠服务器", 8760, 2),  # MTTF=1年, MTTR=2小时
        ("云实例自动恢复", 2160, 0.5),  # MTTF=90天, MTTR=30分钟
    ]
    print(f"{'场景':<16} {'MTTF(h)':<10} {'MTTR(h)':<10} {'可用性':<10} {'年停机'}")
    print("-" * 60)
    for name, mttf, mttr in scenarios:
        r = calc_availability(mttf, mttr)
        print(f"{name:<16} {mttf:<10} {mttr:<10}"
              f" {r['availability_pct']}%  {r['downtime_year_min']} 分钟/年")

    print("\n--- 3. 冗余策略对比 ---")
    for s in redundancy_strategies():
        print(f"\n  {s['name']}")
        print(f"    描述: {s['desc']}")
        print(f"    可用性: {s['availability_pct']}%")
        dt = (1 - s['availability_pct'] / 100) * 365 * 24 * 60
        print(f"    年停机: {dt:.1f} 分钟 | 成本: {s['cost']} | 复杂度: {s['complexity']}")

    print("\n--- 4. 级联可用性 ---")
    # 模拟一个完整服务链路
    components = [
        ("CDN", 0.9999),
        ("负载均衡", 0.9999),
        ("Web 服务", 0.999),
        ("缓存", 0.9995),
        ("数据库", 0.999),
    ]
    print(f"{'组件':<14} {'可用性':<10}")
    print("-" * 26)
    for name, avail in components:
        print(f"{name:<14} {avail:<10.4%}")
    composite = calc_composite_availability(components)
    print("-" * 26)
    print(f"{'整体':<14} {composite:<10.4%}")
    dt = (1 - composite) * 365 * 24 * 60
    print(f"\n整体年停机: {dt:.1f} 分钟")
    print(f"要达到 99.9%+ 可用性，最薄弱环节必须加固")

    print("\n" + "=" * 56)
    print("提示: 高可用设计需权衡成本与复杂度")
    print("=" * 56)
