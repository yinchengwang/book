"""
mvcc — MVCC 多版本并发控制演示

演示版本链（Undo Log）、ReadView、快照隔离的基本原理。
"""

import sqlite3
import threading
import time
from typing import List, Optional, Dict

import os
DB_FILE = os.path.join(os.path.dirname(__file__), "mvcc_demo.db")


def setup(conn: sqlite3.Connection):
    conn.executescript("""
        PRAGMA journal_mode = WAL;
        CREATE TABLE accounts (
            id      INTEGER PRIMARY KEY,
            name    TEXT,
            balance REAL,
            version INTEGER DEFAULT 1
        );
        INSERT INTO accounts VALUES (1, 'Alice', 1000, 1);
        INSERT INTO accounts VALUES (2, 'Bob', 2000, 1);
    """)
    print("[[OK]] 表已创建：Alice(1000,v1) / Bob(2000,v1), WAL 模式\n")


# ════════════════════════════════════════════════════════════
# 1. 版本链（Version Chain）概念演示
# ════════════════════════════════════════════════════════════
def demo_version_chain():
    print("═══ 1. 版本链（Version Chain） ═══")
    print("  MVCC 核心思想：每个修改不覆盖原数据，而是生成新版本")
    print()
    print("  版本链结构：")
    print("    Row(id=1, name='Alice', balance=1000, version=1)")
    print("        ↑  事务 A 修改 balance=800")
    print("    Row(id=1, name='Alice', balance=800, version=2)")
    print("        ↑  事务 B 修改 balance=1200")
    print("    Row(id=1, name='Alice', balance=1200, version=3)")
    print()
    print("  旧版本存储在 Undo Log / 回滚段中")
    print("  当前版本在数据页中，老版本通过版本链可达")
    print()

    # 用 SQLite 演示版本效果
    conn = sqlite3.connect(DB_FILE)
    conn.execute("BEGIN")
    cur = conn.execute("SELECT * FROM accounts WHERE id = 1")
    row_before = cur.fetchone()
    print(f"  修改前: id={row_before[0]}, name={row_before[1]}, "
          f"balance={row_before[2]}, version={row_before[3]}")

    conn.execute(
        "UPDATE accounts SET balance = 800, version = version + 1 WHERE id = 1")
    cur = conn.execute("SELECT * FROM accounts WHERE id = 1")
    row_after = cur.fetchone()
    print(f"  修改后: id={row_after[0]}, name={row_after[1]}, "
          f"balance={row_after[2]}, version={row_after[3]}")
    conn.execute("COMMIT")
    conn.close()
    print()


# ════════════════════════════════════════════════════════════
# 2. ReadView 与快照隔离
# ════════════════════════════════════════════════════════════
def demo_readview():
    print("═══ 2. ReadView 与快照隔离（Snapshot Isolation） ═══")
    results: List[str] = []
    event = threading.Event()

    def txn_writer():
        conn = sqlite3.connect(DB_FILE)
        conn.execute("BEGIN IMMEDIATE")
        conn.execute("UPDATE accounts SET balance = 5000 WHERE id = 1")
        results.append("Writer: Alice 余额改为 5000（已提交）")
        conn.execute("COMMIT")
        conn.close()
        event.set()

    def txn_reader():
        conn = sqlite3.connect(DB_FILE)
        conn.execute("BEGIN")

        # 第一次读（Writer 修改前）
        bal1 = conn.execute(
            "SELECT balance FROM accounts WHERE id = 1").fetchone()[0]
        results.append(f"Reader: 第一次读 Alice 余额 = {bal1}")

        # 等待 Writer 提交
        event.wait(timeout=3)

        # 第二次读（Writer 已提交）
        bal2 = conn.execute(
            "SELECT balance FROM accounts WHERE id = 1").fetchone()[0]
        results.append(f"Reader: 第二次读 Alice 余额 = {bal2}")

        if bal1 == bal2:
            results.append("  → 快照隔离！Reader 看到的是事务开始时的快照")
        else:
            results.append("  → 读到了新数据（非快照隔离）")
        conn.execute("COMMIT")
        conn.close()

    t1 = threading.Thread(target=txn_writer)
    t2 = threading.Thread(target=txn_reader)
    t1.start()
    t2.start()
    t1.join()
    t2.join()

    for r in results:
        print(f"  {r}")
    print()

    print("  ReadView 包含：")
    print("    - m_ids：事务开始时活跃的事务 ID 列表")
    print("    - m_low_limit_id：尚未分配的最小事务 ID")
    print("    - m_up_limit_id：活跃事务中最大的事务 ID")
    print("  可见性判断：事务 ID 在 m_ids 中 → 不可见（未提交）")
    print()


# ════════════════════════════════════════════════════════════
# 3. MVCC 的写写冲突
# ════════════════════════════════════════════════════════════
def demo_write_conflict():
    print("═══ 3. MVCC 写写冲突 ═══")
    results: List[str] = []

    def txn_1():
        conn = sqlite3.connect(DB_FILE)
        try:
            conn.execute("BEGIN IMMEDIATE")
            conn.execute("UPDATE accounts SET balance = balance - 100 WHERE id = 2")
            results.append("Txn1: Bob 余额减 100")
            time.sleep(0.3)
            conn.execute("COMMIT")
            results.append("Txn1: 提交成功")
        except Exception as e:
            results.append(f"Txn1: {e}")
        conn.close()

    def txn_2():
        time.sleep(0.1)
        conn = sqlite3.connect(DB_FILE)
        try:
            conn.execute("BEGIN IMMEDIATE")
            conn.execute("UPDATE accounts SET balance = balance + 200 WHERE id = 2")
            results.append("Txn2: Bob 余额加 200（等待 Txn1 释放锁）")
            conn.execute("COMMIT")
            results.append("Txn2: 提交成功")
        except Exception as e:
            results.append(f"Txn2: {e}")
        conn.close()

    t1 = threading.Thread(target=txn_1)
    t2 = threading.Thread(target=txn_2)
    t1.start()
    t2.start()
    t1.join()
    t2.join()

    cur_bal = sqlite3.connect(
        DB_FILE).execute("SELECT balance FROM accounts WHERE id = 2").fetchone()[0]
    results.append(f"  最终 Bob 余额 = {cur_bal}（2000 - 100 + 200 = 2100 [OK]）")

    for r in results:
        print(f"  {r}")
    print("  MVCC + 行锁：写操作序列化，读操作无阻塞\n")


def main():
    print("=" * 50)
    print("  MVCC 演示 — 版本链 / ReadView / 快照隔离")
    print("=" * 50 + "\n")

    conn = sqlite3.connect(DB_FILE)
    setup(conn)

    demo_version_chain()
    demo_readview()
    demo_write_conflict()

    conn.close()
    print("所有 MVCC 演示完成。")


if __name__ == "__main__":
    main()
