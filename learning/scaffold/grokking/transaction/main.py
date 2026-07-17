"""
transaction — 事务演示

演示 ACID 特性、四种隔离级别以及并发事务的典型问题。
"""

import sqlite3
import threading
import time
from typing import Optional

import os
DB_FILE = os.path.join(os.path.dirname(__file__), "txn_demo.db")


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
    """)
    print("[[OK]] 账户表已创建：Alice(1000) / Bob(1000) / Charlie(1000)\n")


# ════════════════════════════════════════════════════════════
# 1. ACID —— 原子性（Atomicity）
# ════════════════════════════════════════════════════════════
def demo_atomicity(conn: sqlite3.Connection):
    print("═══ 1. 原子性（Atomicity）═══")
    try:
        conn.execute("BEGIN")
        conn.execute("UPDATE accounts SET balance = balance - 200 WHERE id = 1")
        # 模拟失败：无效的 SQL
        conn.execute("UPDATE accounts SET balance = balance + 200 WHERE id = 99999")
        conn.execute("COMMIT")
    except Exception as e:
        conn.execute("ROLLBACK")
        print(f"  事务失败，回滚。错误: {e}")

    bal = conn.execute("SELECT balance FROM accounts WHERE id = 1").fetchone()[0]
    print(f"  Alice 余额: {bal}（正确：200 不会被扣减）\n")


# ════════════════════════════════════════════════════════════
# 2. 隔离性 —— 并发问题演示
# ════════════════════════════════════════════════════════════
def demo_concurrency_issues():
    print("═══ 2. 并发问题 ═══")

    # 脏读演示（Read Uncommitted）
    print("  --- 脏读 Dirty Read ---")
    conn_main = sqlite3.connect(DB_FILE)
    conn_main.executescript("""
        UPDATE accounts SET balance = 1000 WHERE id = 1;
        UPDATE accounts SET balance = 1000 WHERE id = 2;
    """)
    conn_main.commit()
    conn_main.close()

    results = []

    def txn_dirty_writer():
        conn = sqlite3.connect(DB_FILE)
        conn.execute("BEGIN")
        conn.execute("UPDATE accounts SET balance = 500 WHERE id = 1")
        # 未提交就读到修改
        results.append("Writer: 修改 Alice 余额为 500（未提交）")
        time.sleep(0.1)
        conn.execute("ROLLBACK")
        results.append("Writer: 回滚！Alice 恢复 1000")
        conn.close()

    def txn_dirty_reader():
        time.sleep(0.05)
        conn = sqlite3.connect(DB_FILE)
        bal = conn.execute("SELECT balance FROM accounts WHERE id = 1").fetchone()[0]
        results.append(f"Reader: 读到 Alice 余额 {bal}（SQLite 默认不回读未提交写）")
        conn.close()

    t1 = threading.Thread(target=txn_dirty_writer)
    t2 = threading.Thread(target=txn_dirty_reader)
    t1.start()
    t2.start()
    t1.join()
    t2.join()

    for r in results:
        print(f"  {r}")
    print()

    # 不可重复读演示
    print("  --- 不可重复读 Non-Repeatable Read ---")
    results.clear()

    def txn_updater():
        time.sleep(0.15)
        conn = sqlite3.connect(DB_FILE)
        conn.execute("BEGIN")
        conn.execute("UPDATE accounts SET balance = 1500 WHERE id = 2")
        conn.execute("COMMIT")
        results.append("Updater: Bob 余额更新为 1500（已提交）")
        conn.close()

    def txn_repeat_reader():
        conn = sqlite3.connect(DB_FILE)
        conn.execute("BEGIN")
        bal1 = conn.execute(
            "SELECT balance FROM accounts WHERE id = 2").fetchone()[0]
        results.append(f"Reader: 第一次读 Bob 余额 = {bal1}")
        time.sleep(0.3)
        bal2 = conn.execute(
            "SELECT balance FROM accounts WHERE id = 2").fetchone()[0]
        results.append(f"Reader: 第二次读 Bob 余额 = {bal2}")
        results.append(
            f"  {'[OK] 相同（可重复读）' if bal1 == bal2 else '[NO] 不同（不可重复读！）'}")
        conn.execute("COMMIT")
        conn.close()

    t1 = threading.Thread(target=txn_updater)
    t2 = threading.Thread(target=txn_repeat_reader)
    t1.start()
    t2.start()
    t1.join()
    t2.join()

    for r in results:
        print(f"  {r}")
    print()


# ════════════════════════════════════════════════════════════
# 3. 隔离级别 —— 显式控制
# ════════════════════════════════════════════════════════════
def demo_isolation_levels(conn: sqlite3.Connection):
    print("═══ 3. 隔离级别 ═══")

    conn.execute("PRAGMA read_uncommitted = 0;")
    cur = conn.execute("PRAGMA read_uncommitted;")
    print(f"  SQLite 隔离级别: read_uncommitted = {cur.fetchone()[0]}")
    print("  SQLite 默认为 Serializable（通过锁实现）")
    print("  标准隔离级别：")
    levels = [
        ("READ UNCOMMITTED", "最低等，有脏读"),
        ("READ COMMITTED", "无脏读，有不可重复读"),
        ("REPEATABLE READ", "无脏读和不可重复读，有幻读"),
        ("SERIALIZABLE", "最高等，全部避免"),
    ]
    for name, desc in levels:
        print(f"    {name:20s} — {desc}")
    print()


# ════════════════════════════════════════════════════════════
# 4. 持久性（Durability）
# ════════════════════════════════════════════════════════════
def demo_durability(conn: sqlite3.Connection):
    print("═══ 4. 持久性（Durability）═══")
    conn.execute("PRAGMA synchronous = FULL")
    conn.execute("PRAGMA journal_mode = WAL")
    cur = conn.execute("PRAGMA journal_mode")
    print(f"  日志模式: {cur.fetchone()[0]} — WAL 确保提交的事务不会丢失")
    print()


def main():
    print("=" * 50)
    print("  事务演示 — ACID / 隔离级别 / 并发问题")
    print("=" * 50 + "\n")

    conn = sqlite3.connect(DB_FILE)
    setup(conn)

    demo_atomicity(conn)
    demo_concurrency_issues()
    demo_isolation_levels(conn)
    demo_durability(conn)

    conn.close()
    print("所有事务演示完成。")


if __name__ == "__main__":
    main()
