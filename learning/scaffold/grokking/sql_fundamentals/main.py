"""
sql_fundamentals — SQL 基础

演示 SELECT/JOIN/聚合函数/索引 的核心语法和执行流程。
使用 SQLite 作为嵌入式数据库引擎，无需安装独立数据库。
"""

import sqlite3
import time
from typing import List, Tuple

DB_FILE = ":memory:"  # 内存数据库，运行完毕自动清理


def create_tables(conn: sqlite3.Connection):
    """创建演示用的表结构，包含外键关系"""
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS departments (
            id   INTEGER PRIMARY KEY,
            name TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS employees (
            id         INTEGER PRIMARY KEY,
            name       TEXT NOT NULL,
            dept_id    INTEGER REFERENCES departments(id),
            salary     REAL NOT NULL,
            hire_date  TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS projects (
            id          INTEGER PRIMARY KEY,
            title       TEXT NOT NULL,
            emp_id      INTEGER REFERENCES employees(id),
            budget      REAL,
            deadline    TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_emp_dept ON employees(dept_id);
        CREATE INDEX IF NOT EXISTS idx_proj_emp ON projects(emp_id);
    """)
    print("[[OK]] 表结构创建完成：departments / employees / projects")
    print("[[OK]] 索引创建完成：idx_emp_dept / idx_proj_emp\n")


def seed_data(conn: sqlite3.Connection):
    """插入示例数据"""
    conn.executescript("""
        INSERT INTO departments (id, name) VALUES
            (1, '工程部'), (2, '市场部'), (3, '财务部');

        INSERT INTO employees (id, name, dept_id, salary, hire_date) VALUES
            (101, '张三', 1, 25000, '2020-03-15'),
            (102, '李四', 1, 22000, '2021-06-01'),
            (103, '王五', 2, 18000, '2022-01-10'),
            (104, '赵六', 2, 16000, '2022-09-20'),
            (105, '陈七', 3, 20000, '2021-11-05'),
            (106, '刘八', 3, 19000, '2023-02-28');

        INSERT INTO projects (id, title, emp_id, budget, deadline) VALUES
            (1, 'AI 平台',     101, 500000, '2026-06-30'),
            (2, '数据中台',     102, 300000, '2026-08-15'),
            (3, '官网改版',     103, 100000, '2026-05-01'),
            (4, '营销自动化',   104, 200000, '2026-09-30'),
            (5, '财报系统',     105, 150000, '2026-07-01'),
            (6, '审计工具',     106, 120000, '2026-10-15');
    """)
    print("[[OK]] 示例数据插入完成（3 部门、6 员工、6 项目）\n")


# ════════════════════════════════════════════════════════════
# 1. SELECT 基础 —— 投影与过滤
# ════════════════════════════════════════════════════════════
def demo_select(conn: sqlite3.Connection):
    print("═══ 1. SELECT 基础 ═══")
    cur = conn.execute("SELECT id, name, salary FROM employees WHERE salary > 19000")
    rows = cur.fetchall()
    for r in rows:
        print(f"  员工 {r[0]}: {r[1]}，薪资 {r[2]}")
    print()


# ════════════════════════════════════════════════════════════
# 2. JOIN —— 多表关联
# ════════════════════════════════════════════════════════════
def demo_join(conn: sqlite3.Connection):
    print("═══ 2. INNER JOIN ═══")
    cur = conn.execute("""
        SELECT e.name, d.name AS dept, e.salary
        FROM employees e
        JOIN departments d ON e.dept_id = d.id
        ORDER BY e.salary DESC
    """)
    for r in cur.fetchall():
        print(f"  {r[0]:　<6} | {r[1]:　<6} | {r[2]:>5}")
    print()

    print("═══ LEFT JOIN（含无项目的员工）═══")
    cur = conn.execute("""
        SELECT e.name, p.title
        FROM employees e
        LEFT JOIN projects p ON e.id = p.emp_id
    """)
    for r in cur.fetchall():
        proj = r[1] or "(无项目)"
        print(f"  {r[0]:　<6} → {proj}")
    print()


# ════════════════════════════════════════════════════════════
# 3. 聚合函数 —— GROUP BY / HAVING
# ════════════════════════════════════════════════════════════
def demo_aggregation(conn: sqlite3.Connection):
    print("═══ 3. 聚合查询 ═══")
    cur = conn.execute("""
        SELECT d.name,
               COUNT(e.id)    AS 人数,
               ROUND(AVG(e.salary), 0) AS 平均薪资,
               SUM(e.salary)  AS 薪资总额
        FROM departments d
        JOIN employees e ON d.id = e.dept_id
        GROUP BY d.id
        HAVING 人数 >= 2
        ORDER BY 平均薪资 DESC
    """)
    for r in cur.fetchall():
        print(f"  {r[0]:　<6} | 人数 {r[1]} | 平均 {r[2]} | 总额 {r[3]}")
    print()


# ════════════════════════════════════════════════════════════
# 4. 子查询与标量子查询
# ════════════════════════════════════════════════════════════
def demo_subquery(conn: sqlite3.Connection):
    print("═══ 4. 子查询 ═══")
    cur = conn.execute("""
        SELECT name, salary
        FROM employees
        WHERE salary > (SELECT AVG(salary) FROM employees)
    """)
    for r in cur.fetchall():
        print(f"  {r[0]:　<6} 薪资 {r[1]}（高于平均水平）")
    print()


# ════════════════════════════════════════════════════════════
# 5. 索引效果演示
# ════════════════════════════════════════════════════════════
def demo_index(conn: sqlite3.Connection):
    print("═══ 5. 索引效果对比 ═══")

    def query_no_index(dept_name: str):
        """无索引查询：全表扫描"""
        start = time.perf_counter()
        conn.execute("""
            SELECT e.* FROM employees e, departments d
            WHERE e.dept_id = d.id AND d.name = ?
        """, (dept_name,)).fetchall()
        return time.perf_counter() - start

    def query_with_index(dept_name: str):
        """有索引查询：通过 idx_emp_dept 定位"""
        start = time.perf_counter()
        conn.execute("""
            SELECT * FROM employees
            WHERE dept_id = (SELECT id FROM departments WHERE name = ?)
        """, (dept_name,)).fetchall()
        return time.perf_counter() - start

    # 插入大量数据使索引效果可测量
    many: List[Tuple[int, str, int, float, str]] = []
    for i in range(5000):
        many.append((200 + i, f"User{i}", 1, 10000 + i, "2024-01-01"))
    conn.executemany(
        "INSERT INTO employees VALUES (?,?,?,?,?)", many
    )
    conn.execute("ANALYZE")

    t1 = query_no_index("工程部")
    t2 = query_with_index("工程部")
    print(f"  全表扫描: {t1*1000:.2f} ms")
    print(f"  索引查找: {t2*1000:.2f} ms")
    print(f"  加速比:   {t1/t2:.1f}x")
    print()


# ════════════════════════════════════════════════════════════
# 入口
# ════════════════════════════════════════════════════════════
def main():
    print("=" * 50)
    print("  SQL 基础演示 — SELECT / JOIN / 聚合 / 索引")
    print("=" * 50 + "\n")

    conn = sqlite3.connect(DB_FILE)
    create_tables(conn)
    seed_data(conn)

    demo_select(conn)
    demo_join(conn)
    demo_aggregation(conn)
    demo_subquery(conn)
    demo_index(conn)

    conn.close()
    print("所有演示完成。")


if __name__ == "__main__":
    main()
