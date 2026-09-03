"""
normalization — 数据库范式演示

演示 1NF/2NF/3NF/BCNF 的逐步规范化过程以及反规范化的典型场景。
"""

import sqlite3
from typing import List, Tuple

DB_FILE = ":memory:"


# ════════════════════════════════════════════════════════════
# 1NF —— 第一范式：原子列、无重复组
# ════════════════════════════════════════════════════════════
def demo_1nf(conn: sqlite3.Connection):
    print("═══ 1NF — 第一范式（原子性）═══")
    conn.executescript("""
        -- 违反 1NF：phones 字段包含多个值
        CREATE TABLE employees_non_1nf (
            id     INTEGER PRIMARY KEY,
            name   TEXT,
            phones TEXT   -- 问题：存储 "138xxx,139xxx" 多个号码
        );
        INSERT INTO employees_non_1nf VALUES
            (1, '张三', '13800138000,13900139000'),
            (2, '李四', '13700137000');

        -- 符合 1NF：每个值占一行
        CREATE TABLE employees_1nf (
            id     INTEGER,
            name   TEXT,
            phone  TEXT,
            PRIMARY KEY (id, phone)
        );
        INSERT INTO employees_1nf VALUES
            (1, '张三', '13800138000'),
            (1, '张三', '13900139000'),
            (2, '李四', '13700137000');
    """)
    cur = conn.execute("SELECT * FROM employees_non_1nf")
    print("  违反 1NF：phones 含多个值")
    for r in cur.fetchall():
        print(f"    {r[0]}: {r[1]} → {r[2]}")

    cur = conn.execute("SELECT * FROM employees_1nf")
    print("  符合 1NF：phone 原子化")
    for r in cur.fetchall():
        print(f"    {r[0]}: {r[1]} → {r[2]}")
    print()


# ════════════════════════════════════════════════════════════
# 2NF —— 第二范式：消除部分依赖
# ════════════════════════════════════════════════════════════
def demo_2nf(conn: sqlite3.Connection):
    print("═══ 2NF — 第二范式（消除部分函数依赖）═══")
    conn.executescript("""
        -- 违反 2NF：主键 (emp_id, dept_id)，但 dept_name 只依赖于 dept_id（部分依赖）
        CREATE TABLE emp_dept_non_2nf (
            emp_id    INTEGER,
            dept_id   INTEGER,
            emp_name  TEXT,
            dept_name TEXT,     -- 部分依赖于 dept_id，而非整个主键
            PRIMARY KEY (emp_id, dept_id)
        );

        -- 符合 2NF：拆分出 department 表
        CREATE TABLE employees_2nf (
            emp_id   INTEGER PRIMARY KEY,
            emp_name TEXT,
            dept_id  INTEGER
        );
        CREATE TABLE departments_2nf (
            dept_id   INTEGER PRIMARY KEY,
            dept_name TEXT
        );
    """)
    print("  违反 2NF：dept_name 部分依赖于主键的一部分")
    print("  符合 2NF：拆分出独立的 departments 表")
    print()


# ════════════════════════════════════════════════════════════
# 3NF —— 第三范式：消除传递依赖
# ════════════════════════════════════════════════════════════
def demo_3nf(conn: sqlite3.Connection):
    print("═══ 3NF — 第三范式（消除传递函数依赖）═══")
    conn.executescript("""
        -- 违反 3NF：emp_id → dept_city 通过 dept_id 传递
        CREATE TABLE emp_non_3nf (
            emp_id    INTEGER PRIMARY KEY,
            emp_name  TEXT,
            dept_id   INTEGER,
            dept_city TEXT   -- 传递依赖于 emp_id：emp_id → dept_id → dept_city
        );

        -- 符合 3NF：部门信息独立到 departments 表
        CREATE TABLE employees_3nf (
            emp_id   INTEGER PRIMARY KEY,
            emp_name TEXT,
            dept_id  INTEGER
        );
        CREATE TABLE departments_3nf (
            dept_id   INTEGER PRIMARY KEY,
            dept_name TEXT,
            city      TEXT
        );
    """)
    print("  违反 3NF：dept_city 传递依赖于 emp_id")
    print("  符合 3NF：部门城市移到 departments 表")
    print()


# ════════════════════════════════════════════════════════════
# BCNF —— 鲍依斯-科得范式
# ════════════════════════════════════════════════════════════
def demo_bcnf(conn: sqlite3.Connection):
    print("═══ BCNF — 鲍依斯-科得范式 ═══")
    conn.executescript("""
        -- 违反 BCNF：主键 (student, course)，但 professor → course（决定子不是候选键超集）
        -- 场景：每门课只有一个教授，但一个教授可以教多门课
        CREATE TABLE enrollment_non_bcnf (
            student   TEXT,
            course    TEXT,
            professor TEXT,
            PRIMARY KEY (student, course)
        );
        INSERT INTO enrollment_non_bcnf VALUES
            ('Alice', '数学', '张教授'),
            ('Bob',   '数学', '张教授'),
            ('Alice', '物理', '李教授');

        -- 符合 BCNF：拆分为 course_professor 和 student_course
        CREATE TABLE course_professor (
            course    TEXT PRIMARY KEY,
            professor TEXT
        );
        CREATE TABLE student_course (
            student TEXT,
            course  TEXT,
            PRIMARY KEY (student, course)
        );
    """)
    cur = conn.execute("SELECT * FROM enrollment_non_bcnf")
    print("  违反 BCNF：professor 是 course 的决定子但不是超键")
    for r in cur.fetchall():
        print(f"    {r[0]} → {r[1]} → {r[2]}")
    print()


# ════════════════════════════════════════════════════════════
# 反规范化 —— 性能驱动的故意冗余
# ════════════════════════════════════════════════════════════
def demo_denormalization(conn: sqlite3.Connection):
    print("═══ 反规范化（Denormalization）═══")
    conn.executescript("""
        -- 规范化设计：查询员工部门名需要 JOIN
        CREATE TABLE emp_norm (
            emp_id   INTEGER PRIMARY KEY,
            emp_name TEXT,
            dept_id  INTEGER
        );
        CREATE TABLE dept_norm (
            dept_id   INTEGER PRIMARY KEY,
            dept_name TEXT
        );

        -- 反规范化：直接将部门名冗余到员工表，避免 JOIN
        CREATE TABLE emp_denorm (
            emp_id    INTEGER PRIMARY KEY,
            emp_name  TEXT,
            dept_id   INTEGER,
            dept_name TEXT   -- 冗余字段
        );
    """)
    print("  规范化：emp_norm JOIN dept_norm 获取部门名")
    print("  反规范化：emp_denorm 冗余 dept_name，避免 JOIN")
    print("  适用场景：查询量大、部门名极少变更的报表系统")
    print()


def main():
    print("=" * 50)
    print("  数据库范式演示 — 1NF / 2NF / 3NF / BCNF / 反规范化")
    print("=" * 50 + "\n")

    conn = sqlite3.connect(DB_FILE)

    demo_1nf(conn)
    demo_2nf(conn)
    demo_3nf(conn)
    demo_bcnf(conn)
    demo_denormalization(conn)

    conn.close()
    print("所有范式演示完成。")


if __name__ == "__main__":
    main()
