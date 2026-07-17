"""
lock_manager — 锁管理器演示

演示行锁/表锁/意向锁/死锁检测/锁升级的基本原理。
"""

import sqlite3
import threading
import time
from typing import List, Optional

import os
DB_FILE = os.path.join(os.path.dirname(__file__), "lock_demo.db")


def setup(conn: sqlite3.Connection):
    conn.executescript("""
        CREATE TABLE accounts (
            id      INTEGER PRIMARY KEY,
            name    TEXT,
            balance REAL
        );
        INSERT INTO accounts VALUES (1, 'Alice', 1000);
        INSERT INTO accounts VALUES (2, 'Bob', 1000);
        INSERT INTO accounts VALUES (3, 'Charlie', 1000);
        INSERT INTO accounts VALUES (4, 'David', 1000);
    """)
    print("[[OK]] 账户表已创建，4 个账户各 1000\n")


# ════════════════════════════════════════════════════════════
# 1. 行锁 vs 表锁
# ════════════════════════════════════════════════════════════
def demo_row_vs_table():
    print("═══ 1. 行锁 vs 表锁 ═══")
    results: List[str] = []

    def txn1():
        conn = sqlite3.connect(DB_FILE)
        conn.execute("BEGIN")
        conn.execute("UPDATE accounts SET balance = balance - 100 WHERE id = 1")
        results.append("Txn1: 锁住 Alice 行（id=1），修改余额")
        time.sleep(0.3)
        conn.execute("COMMIT")
        results.append("Txn1: 提交")
        conn.close()

    def txn2():
        time.sleep(0.1)
        conn = sqlite3.connect(DB_FILE)
        conn.execute("BEGIN")
        start = time.perf_counter()
        conn.execute("UPDATE accounts SET balance = balance + 50 WHERE id = 2")
        elapsed = (time.perf_counter() - start) * 1000
        results.append(
            f"Txn2: 更新 Bob 行（id=2），无阻塞，耗时 {elapsed:.0f} ms")
        conn.execute("COMMIT")
        conn.close()

    t1 = threading.Thread(target=txn1)
    t2 = threading.Thread(target=txn2)
    t1.start()
    t2.start()
    t1.join()
    t2.join()

    for r in results:
        print(f"  {r}")
    print("  → 行级锁：不同行可并发修改（SQLite 默认行锁）\n")


# ════════════════════════════════════════════════════════════
# 2. 意向锁 —— 表级锁与行级锁的协同
# ════════════════════════════════════════════════════════════
def demo_intention_lock():
    print("═══ 2. 意向锁（Intention Lock）概念 ═══")
    print("  意向锁不是一种实际锁，而是一种【标记】")
    print("  事务在获取行锁前，先在表级别设置意向锁")
    print()
    print("  锁兼容性矩阵：")
    print("  ┌───────────┬───┬───┬───┬───┐")
    print("  │           │ IS│ IX│ S │ X │")
    print("  ├───────────┼───┼───┼───┼───┤")
    print("  │ IS(意向读)│ [OK] │ [OK] │ [OK] │ [NO] │")
    print("  │ IX(意向写)│ [OK] │ [OK] │ [NO] │ [NO] │")
    print("  │ S (表读)  │ [OK] │ [NO] │ [OK] │ [NO] │")
    print("  │ X (表写)  │ [NO] │ [NO] │ [NO] │ [NO] │")
    print()

    print("  场景：Txn1 拿到 id=1 的写锁（行锁）")
    print("  在行锁之前，Txn1 先在表级别加 IX（意向写锁）")
    print("  若有 Txn2 想对整个表加 S（表读锁），检查到 IX → 不兼容 → 阻塞")
    print("  层检查快速失败，无需逐行检查！\n")


# ════════════════════════════════════════════════════════════
# 3. 死锁检测
# ════════════════════════════════════════════════════════════
def demo_deadlock():
    print("═══ 3. 死锁（Deadlock）检测 ═══")
    results: List[str] = []

    def txn_a():
        conn = sqlite3.connect(DB_FILE)
        try:
            conn.execute("BEGIN")
            conn.execute("UPDATE accounts SET balance = balance - 100 WHERE id = 1")
            results.append("TxnA: 锁住 Alice (id=1)")
            time.sleep(0.2)
            conn.execute("UPDATE accounts SET balance = balance + 100 WHERE id = 2")
            results.append("TxnA: 锁住 Bob (id=2)")
            conn.execute("COMMIT")
            results.append("TxnA: 提交成功")
        except Exception as e:
            results.append(f"TxnA: 错误 → {e}")
        conn.close()

    def txn_b():
        conn = sqlite3.connect(DB_FILE)
        try:
            conn.execute("BEGIN")
            conn.execute("UPDATE accounts SET balance = balance - 200 WHERE id = 2")
            results.append("TxnB: 锁住 Bob (id=2)")
            time.sleep(0.2)
            conn.execute("UPDATE accounts SET balance = balance + 200 WHERE id = 1")
            results.append("TxnB: 锁住 Alice (id=1)")
            conn.execute("COMMIT")
            results.append("TxnB: 提交成功")
        except Exception as e:
            results.append(f"TxnB: 错误 → {e}")
        conn.close()

    t1 = threading.Thread(target=txn_a)
    t2 = threading.Thread(target=txn_b)
    t1.start()
    t2.start()
    t1.join()
    t2.join()

    for r in results:
        print(f"  {r}")
    print("  → 死锁：TxnA 等 TxnB 释放 id=2，TxnB 等 TxnA 释放 id=1")
    print("  → SQLite 默认超时机制（busy_timeout）可缓解但不可完全避免\n")


# ════════════════════════════════════════════════════════════
# 4. 锁超时与死锁避免
# ════════════════════════════════════════════════════════════
def demo_lock_timeout():
    print("═══ 4. 锁超时与死锁避免策略 ═══")
    strategies = [
        ("锁超时", "SET busy_timeout=5000，等待 5s 后放弃"),
        ("固定顺序", "所有事务按 id 升序加锁，避免循环等待"),
        ("一次性锁定", "BEGIN 时申请所有需要的锁，等齐才执行"),
        ("trylock", "尝试加锁，立即返回，失败时回滚重试"),
    ]
    for name, desc in strategies:
        print(f"  {name:12s} — {desc}")
    print()


def main():
    print("=" * 50)
    print("  锁管理器演示 — 行锁 / 意向锁 / 死锁")
    print("=" * 50 + "\n")

    conn = sqlite3.connect(DB_FILE)
    setup(conn)

    demo_row_vs_table()
    demo_intention_lock()
    demo_deadlock()
    demo_lock_timeout()

    conn.close()
    print("所有锁演示完成。")


if __name__ == "__main__":
    main()
