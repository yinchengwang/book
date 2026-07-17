"""
index_design — 索引设计演示

演示 BTree 索引 / Hash 索引 / 复合索引 / 最左前缀原则。
"""

import sqlite3
import time
from typing import List

DB_FILE = ":memory:"


def create_tables(conn: sqlite3.Connection):
    conn.executescript("""
        CREATE TABLE users (
            id        INTEGER PRIMARY KEY,
            last_name  TEXT NOT NULL,
            first_name TEXT NOT NULL,
            age       INTEGER,
            city      TEXT,
            email     TEXT
        );
        CREATE TABLE orders (
            id       INTEGER PRIMARY KEY,
            user_id  INTEGER,
            amount   REAL,
            status   TEXT,
            created  TEXT
        );
    """)

    # 插入 10000 条数据
    users: List[tuple] = []
    names = ["张", "李", "王", "赵", "陈", "刘", "周", "吴", "郑", "孙"]
    for i in range(10000):
        users.append((i + 1, names[i % 10], f"Name{i}", 20 + i % 40,
                       f"City{i % 50}", f"user{i}@test.com"))
    conn.executemany("INSERT INTO users VALUES (?,?,?,?,?,?)", users)

    orders: List[tuple] = []
    for i in range(20000):
        orders.append((i + 1, (i % 10000) + 1, 10.0 + i * 0.5,
                       "completed" if i % 3 else "pending", f"2026-0{(i % 9) + 1}-01"))
    conn.executemany("INSERT INTO orders VALUES (?,?,?,?,?)", orders)
    print(f"[[OK]] 插入 {len(users)} 用户 + {len(orders)} 订单\n")


# ════════════════════════════════════════════════════════════
# 1. BTree 索引 —— 等值与范围查询
# ════════════════════════════════════════════════════════════
def demo_btree(conn: sqlite3.Connection):
    print("═══ 1. BTree 索引 — 等值与范围查询 ═══")

    def time_query(sql: str, params: tuple) -> float:
        start = time.perf_counter()
        conn.execute(sql, params).fetchall()
        return time.perf_counter() - start

    t_before = time_query(
        "SELECT * FROM users WHERE last_name = ?", ("张",))
    conn.execute("CREATE INDEX IF NOT EXISTS idx_users_last ON users(last_name)")
    conn.execute("ANALYZE")
    t_after = time_query(
        "SELECT * FROM users WHERE last_name = ?", ("张",))

    print(f"  等值查询（无索引）: {t_before*1000:.3f} ms")
    print(f"  等值查询（有索引）: {t_after*1000:.3f} ms")
    print(f"  范围查询（有索引）: ", end="")
    t_range = time_query(
        "SELECT * FROM users WHERE id BETWEEN 100 AND 200", ())
    print(f"{t_range*1000:.3f} ms\n")


# ════════════════════════════════════════════════════════════
# 2. Hash 索引 —— 精确查找
# ════════════════════════════════════════════════════════════
def demo_hash(conn: sqlite3.Connection):
    print("═══ 2. Hash 索引 — 精确查找（SQLite 无原生 Hash，用演示模拟）═══")

    conn.execute("CREATE INDEX IF NOT EXISTS idx_users_email ON users(email)")
    conn.execute("ANALYZE")

    start = time.perf_counter()
    rows = conn.execute(
        "SELECT * FROM users WHERE email = ?", ("user999@test.com",)).fetchall()
    elapsed = (time.perf_counter() - start) * 1000

    print(f"  email 精确匹配: {elapsed:.3f} ms, 命中 {len(rows)} 条")
    print(f"  说明：BTree 也能做等值查询，Hash 索引理论 O(1) 但 SQLite 默认 BTree")
    print()


# ════════════════════════════════════════════════════════════
# 3. 复合索引与最左前缀原则
# ════════════════════════════════════════════════════════════
def demo_composite(conn: sqlite3.Connection):
    print("═══ 3. 复合索引与最左前缀原则 ═══")

    conn.executescript("""
        CREATE INDEX IF NOT EXISTS idx_users_name ON users(last_name, first_name, age);
        ANALYZE;
    """)

    def explain(sql: str, params: tuple) -> str:
        cur = conn.execute(f"EXPLAIN QUERY PLAN {sql}", params)
        return cur.fetchone()[2]

    # 场景 A：走索引（最左前缀满足）
    plan_a = explain(
        "SELECT * FROM users WHERE last_name = ?", ("张",))
    # 场景 B：走索引（最左前缀的前两列）
    plan_b = explain(
        "SELECT * FROM users WHERE last_name = ? AND first_name = ?", ("张", "Name1"))
    # 场景 C：不走索引（跳过最左列）
    plan_c = explain(
        "SELECT * FROM users WHERE first_name = ?", ("Name1",))
    # 场景 D：走索引（age 不满足最左? 虽然但 last_name+first_name 满足）
    plan_d = explain(
        "SELECT * FROM users WHERE last_name = ? AND age = ?", ("张", 30))

    print(f"  a) WHERE last_name — 走索引: {plan_a}")
    print(f"  b) WHERE last_name+first_name — 走索引: {plan_b}")
    print(f"  c) WHERE first_name — 不走索引（跳过了最左列）: {str(plan_c)[:60]}")
    print(f"  d) WHERE last_name+age — 走索引（但只用 last_name 列）: {plan_d}")

    print("\n  最左前缀原则：复合索引 (a,b,c) 有效于")
    print("    [OK] a | [OK] a,b | [OK] a,b,c")
    print("    [NO] b | [NO] c | [NO] b,c")
    print()


# ════════════════════════════════════════════════════════════
# 4. 索引覆盖（Covering Index）
# ════════════════════════════════════════════════════════════
def demo_covering(conn: sqlite3.Connection):
    print("═══ 4. 索引覆盖（Covering Index）═══")

    def time_query(sql: str, params: tuple) -> float:
        start = time.perf_counter()
        conn.execute(sql, params).fetchall()
        return time.perf_counter() - start

    # 非覆盖：需要回表
    t1 = time_query(
        "SELECT * FROM users WHERE last_name = ?", ("李",))

    # 覆盖索引：查询列全在索引中
    t2 = time_query(
        "SELECT last_name, first_name FROM users WHERE last_name = ?", ("李",))

    print(f"  全列查询（回表）: {t1*1000:.3f} ms")
    print(f"  索引覆盖查询:     {t2*1000:.3f} ms")
    print(f"  覆盖索引避免回表，仅访问索引页面即可返回数据")
    print()


def main():
    print("=" * 50)
    print("  索引设计演示 — BTree / Hash / 复合索引")
    print("=" * 50 + "\n")

    conn = sqlite3.connect(DB_FILE)
    create_tables(conn)

    demo_btree(conn)
    demo_hash(conn)
    demo_composite(conn)
    demo_covering(conn)

    conn.close()
    print("所有索引演示完成。")


if __name__ == "__main__":
    main()
