"""
query_optimization — 查询优化演示

演示 EXPLAIN / 索引覆盖 / 慢查询分析 / 查询计划解读。
"""

import sqlite3
import time
from typing import List

DB_FILE = ":memory:"


def setup(conn: sqlite3.Connection):
    conn.executescript("""
        CREATE TABLE orders (
            id        INTEGER PRIMARY KEY,
            user_id   INTEGER,
            product   TEXT,
            amount    REAL,
            status    TEXT,
            created   TEXT,
            region    TEXT
        );
        CREATE INDEX idx_orders_user ON orders(user_id);
        CREATE INDEX idx_orders_status ON orders(status);
        CREATE INDEX idx_orders_created ON orders(created);
    """)

    orders: List[tuple] = []
    for i in range(20000):
        orders.append((i + 1, (i % 5000) + 1, f"Product{i % 200}",
                       5.0 + (i % 100) * 10.0,
                       "completed" if i % 4 != 0 else "pending",
                       f"2026-{(i % 12) + 1:02d}-{(i % 28) + 1:02d}",
                       ["华东", "华南", "华北", "西部"][i % 4]))
    conn.executemany("INSERT INTO orders VALUES (?,?,?,?,?,?,?)", orders)
    conn.execute("ANALYZE")
    print(f"[[OK]] 插入 {len(orders)} 条订单，索引已创建\n")


# ════════════════════════════════════════════════════════════
# 1. EXPLAIN 查询计划解读
# ════════════════════════════════════════════════════════════
def demo_explain(conn: sqlite3.Connection):
    print("═══ 1. EXPLAIN 查询计划 ═══")
    queries = [
        ("全表扫描", "SELECT * FROM orders WHERE product = 'Product1'"),
        ("索引查找", "SELECT * FROM orders WHERE user_id = 100"),
        ("范围扫描", "SELECT * FROM orders WHERE id BETWEEN 100 AND 200"),
        ("排序和索引", "SELECT * FROM orders ORDER BY created"),
        ("聚合", "SELECT status, COUNT(*) FROM orders GROUP BY status"),
    ]

    for name, sql in queries:
        cur = conn.execute(f"EXPLAIN QUERY PLAN {sql}")
        plan = cur.fetchone()[2]
        print(f"  [{name}]")
        print(f"    SQL: {sql}")
        print(f"    计划: {plan}\n")


# ════════════════════════════════════════════════════════════
# 2. 索引覆盖 vs 回表
# ════════════════════════════════════════════════════════════
def demo_covering(conn: sqlite3.Connection):
    print("═══ 2. 索引覆盖 vs 回表 ═══")

    def time_query(sql: str, params: tuple) -> float:
        start = time.perf_counter()
        conn.execute(sql, params).fetchall()
        elapsed = (time.perf_counter() - start) * 1000
        return elapsed

    # 回表：SELECT * 需要从数据页读取
    t1 = time_query(
        "SELECT * FROM orders WHERE user_id = ?", (100,))

    # 覆盖索引：只查 user_id（索引本身包含该列）
    t2 = time_query(
        "SELECT user_id FROM orders WHERE user_id = ?", (100,))

    print(f"  全列查询（需要回表）: {t1:.3f} ms")
    print(f"  索引覆盖查询（免回表）: {t2:.3f} ms")
    if t1 > 0:
        print(f"  加速比: {t1/max(t2, 0.001):.1f}x")
    print()


# ════════════════════════════════════════════════════════════
# 3. 慢查询分析
# ════════════════════════════════════════════════════════════
def demo_slow_query(conn: sqlite3.Connection):
    print("═══ 3. 慢查询分析与优化 ═══")

    queries = [
        ("慢查询：LIKE %pattern% 无法使用索引",
         "SELECT * FROM orders WHERE product LIKE '%Product1%'"),
        ("优化后：精确条件走索引",
         "SELECT * FROM orders WHERE user_id = 100"),
        ("慢查询：函数包裹列",
         "SELECT * FROM orders WHERE substr(created, 1, 7) = '2026-01'"),
        ("优化后：范围查询走索引",
         "SELECT * FROM orders WHERE created >= '2026-01-01' AND created < '2026-02-01'"),
    ]

    for name, sql in queries:
        start = time.perf_counter()
        rows = conn.execute(sql).fetchall()
        elapsed = (time.perf_counter() - start) * 1000

        cur = conn.execute(f"EXPLAIN QUERY PLAN {sql}")
        plan = cur.fetchone()[2]
        print(f"  [{name}]")
        print(f"    {sql}")
        print(f"  耗时: {elapsed:.3f} ms, 行数: {len(rows)}")
        print(f"  计划: {plan}\n")


# ════════════════════════════════════════════════════════════
# 4. 多条件组合优化
# ════════════════════════════════════════════════════════════
def demo_multi_condition(conn: sqlite3.Connection):
    print("═══ 4. 多条件组合优化 ═══")

    def analyze(sql: str, params: tuple):
        start = time.perf_counter()
        rows = conn.execute(sql, params).fetchall()
        elapsed = (time.perf_counter() - start) * 1000
        cur = conn.execute(f"EXPLAIN QUERY PLAN {sql}", params)
        plan = cur.fetchone()[2]
        print(f"    [{elapsed:6.2f} ms] {str(plan)[:70]}")
        return elapsed

    print("  查询：SELECT * FROM orders WHERE user_id=? AND status=?")
    # 无复合索引：只用一个索引，另一个条件内存过滤
    analyze("SELECT * FROM orders WHERE user_id=? AND status=?",
            (100, "completed"))

    # 创建复合索引
    conn.execute("CREATE INDEX idx_orders_user_status ON orders(user_id, status)")
    conn.execute("ANALYZE")

    print("  （创建复合索引后）")
    # 有复合索引：两个条件都用索引过滤
    analyze("SELECT * FROM orders WHERE user_id=? AND status=?",
            (100, "completed"))

    print()


def main():
    print("=" * 50)
    print("  查询优化演示 — EXPLAIN / 覆盖索引 / 慢查询")
    print("=" * 50 + "\n")

    conn = sqlite3.connect(DB_FILE)
    setup(conn)

    demo_explain(conn)
    demo_covering(conn)
    demo_slow_query(conn)
    demo_multi_condition(conn)

    conn.close()
    print("所有查询优化演示完成。")


if __name__ == "__main__":
    main()
