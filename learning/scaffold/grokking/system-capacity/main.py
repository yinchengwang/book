"""
容量估算 — QPS/TPS 计算 + 存储估算 + 带宽估算

演示内容:
1. QPS/TPS 计算: 按 DAU 和用户行为估算峰值 QPS
2. 存储估算: 按单条数据大小和日增量估算 N 年存储
3. 带宽估算: 按平均响应大小和 QPS 估算网络带宽
"""

# ============================================================================
# 1. QPS/TPS 计算
# ============================================================================

def calc_qps(dau: int, daily_actions: int, peak_factor: float = 5.0,
             seconds_per_day: int = 86400) -> dict:
    """按 DAU 和用户行为估算 QPS

    Args:
        dau: 日活跃用户数
        daily_actions: 每个用户日均操作次数
        peak_factor: 峰值倍数（相对日均）
        seconds_per_day: 一天秒数（默认 86400）

    Returns:
        {"avg_qps": 日均QPS, "peak_qps": 峰值QPS}
    """
    total_daily = dau * daily_actions
    avg_qps = total_daily / seconds_per_day
    peak_qps = avg_qps * peak_factor
    return {"avg_qps": round(avg_qps, 1), "peak_qps": round(peak_qps, 1)}


# ============================================================================
# 2. 存储估算
# ============================================================================

def calc_storage(record_size_bytes: int, daily_records: int, years: int,
                 replication: int = 1, compression_ratio: float = 1.0) -> dict:
    """按单条大小和日增量估算 N 年存储

    Args:
        record_size_bytes: 单条记录字节数
        daily_records: 日增记录数
        years: 存储年限
        replication: 副本数（默认 1）
        compression_ratio: 压缩比（默认 1.0，越小越压缩）

    Returns:
        {"raw_bytes": 原始大小, "raw_pretty": 可读, "with_replication": 含副本}
    """
    raw = record_size_bytes * daily_records * 365 * years
    raw_compress = raw / compression_ratio if compression_ratio > 0 else raw
    total = raw_compress * replication

    def pretty(b):
        for unit in ("B", "KB", "MB", "GB", "TB", "PB"):
            if b < 1024:
                return f"{b:.1f} {unit}"
            b /= 1024
        return f"{b:.1f} EB"

    return {
        "raw_bytes": int(raw),
        "raw_pretty": pretty(raw),
        "compressed_pretty": pretty(raw_compress),
        "with_replication": pretty(total),
    }


# ============================================================================
# 3. 带宽估算
# ============================================================================

def calc_bandwidth(avg_response_bytes: int, peak_qps: float) -> dict:
    """按平均响应大小和峰值 QPS 估算网络带宽

    Returns:
        {"bps": 比特每秒, "mbps": Mbps, "gbps": Gbps}
    """
    bps = avg_response_bytes * 8 * peak_qps
    mbps = bps / 1_000_000
    gbps = bps / 1_000_000_000
    return {"bps": int(bps), "mbps": round(mbps, 1), "gbps": round(gbps, 2)}


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("       容量估算演示 — Capacity Planning")
    print("=" * 56)

    # 场景: 设计一个类似 Twitter 的短消息服务
    DAU = 100_000_000       # 1 亿 DAU
    DAILY_ACTIONS = 200      # 每用户日均 200 次操作（发推、浏览、点赞等）
    RECORD_SIZE = 512        # 每条推文 ~512 B
    DAILY_TWEETS = 500_000_000  # 日增 5 亿条推文
    RESPONSE_SIZE = 2_000    # API 平均响应 ~2 KB

    print(f"\n场景: 短消息服务（1 亿 DAU）")
    print(f"  日活跃用户: {DAU:,}")
    print(f"  每用户日均操作: {DAILY_ACTIONS}")

    # QPS
    q = calc_qps(DAU, DAILY_ACTIONS)
    print(f"\n--- QPS 估算 ---")
    print(f"  日均 QPS: {q['avg_qps']:,.1f}")
    print(f"  峰值 QPS (5×): {q['peak_qps']:,.1f}")

    # 存储
    s = calc_storage(RECORD_SIZE, DAILY_TWEETS, 5, replication=3)
    print(f"\n--- 存储估算（5 年） ---")
    print(f"  日增推文: {DAILY_TWEETS:,}")
    print(f"  原始大小: {s['raw_pretty']}")
    print(f"  压缩后:   {s['compressed_pretty']}")
    print(f"  含 3 副本: {s['with_replication']}")

    # 带宽
    b = calc_bandwidth(RESPONSE_SIZE, q['peak_qps'])
    print(f"\n--- 带宽估算 ---")
    print(f"  平均响应: {RESPONSE_SIZE:,} B")
    print(f"  峰值带宽: {b['mbps']} Mbps / {b['gbps']} Gbps")

    # 对照: 常见网络设备
    print(f"\n--- 网络设备选型对照 ---")
    for dev, cap in [("千兆网卡", 1000), ("万兆网卡", 10000),
                     ("25G 网卡", 25000), ("100G 网卡", 100000)]:
        if b['mbps'] <= cap * 0.8:
            print(f"  v {dev} (<= {cap} Mbps × 80%) 可满足")
        else:
            print(f"  x {dev} (<= {cap} Mbps × 80%) 不足")

    print("\n" + "=" * 56)
    print("提示: 实际容量规划需考虑增长率和冗余")
    print("=" * 56)
