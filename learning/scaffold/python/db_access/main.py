#!/usr/bin/env python3
# db_access scaffold — Python 数据库访问
#
# 复现命令：
#   python3 main.py
#
# 演示 5 段：
#   [connect]    — 连接 SQLite 数据库
#   [create]     — 创建表
#   [insert]     — 插入数据
#   [query]      — 查询数据
#   [transaction]— 事务处理

import sqlite3
import tempfile
import os


def main():
    # 创建临时数据库
    db_path = tempfile.mktemp(suffix='.db')
    print(f"[setup] 数据库: {db_path}")

    # === [connect] 连接 SQLite 数据库 ===
    print("\n[connect] === 连接 SQLite 数据库 ===")

    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    print(f"  连接成功: {db_path}")
    print(f"  SQLite 版本: {sqlite3.sqlite_version}")

    # === [create] 创建表 ===
    print("\n[create] === 创建表 ===")

    cursor.execute('''
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            age INTEGER,
            email TEXT UNIQUE
        )
    ''')

    cursor.execute('''
        CREATE TABLE IF NOT EXISTS posts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER,
            title TEXT NOT NULL,
            content TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        )
    ''')

    conn.commit()
    print("  创建表: users, posts")

    # === [insert] 插入数据 ===
    print("\n[insert] === 插入数据 ===")

    # 单条插入
    cursor.execute(
        "INSERT INTO users (name, age, email) VALUES (?, ?, ?)",
        ("Alice", 30, "alice@example.com")
    )
    conn.commit()
    print(f"  插入用户，ID: {cursor.lastrowid}")

    # 批量插入
    users = [
        ("Bob", 25, "bob@example.com"),
        ("Charlie", 35, "charlie@example.com"),
        ("Diana", 28, "diana@example.com"),
    ]
    cursor.executemany(
        "INSERT INTO users (name, age, email) VALUES (?, ?, ?)",
        users
    )
    conn.commit()
    print(f"  批量插入: {len(users)} 条")

    # === [query] 查询数据 ===
    print("\n[query] === 查询数据 ===")

    # 查询所有
    cursor.execute("SELECT * FROM users")
    rows = cursor.fetchall()
    print(f"  查询所有用户: {len(rows)} 条")
    for row in rows:
        print(f"    {row}")

    # 条件查询
    cursor.execute("SELECT * FROM users WHERE age >= ?", (30,))
    adults = cursor.fetchall()
    print(f"  年龄 >= 30: {len(adults)} 人")
    for user in adults:
        print(f"    {user[1]}, {user[2]} 岁")

    # === [transaction] 事务处理 ===
    print("\n[transaction] === 事务处理 ===")

    # 插入帖子（需要用户ID）
    try:
        cursor.execute(
            "INSERT INTO posts (user_id, title, content) VALUES (?, ?, ?)",
            (1, "Hello World", "This is my first post!")
        )

        # 模拟错误，回滚事务
        cursor.execute("INSERT INTO posts (user_id, title) VALUES (?, ?)", (999, "Orphan Post"))
        # 外键约束会失败

    except sqlite3.IntegrityError as e:
        print(f"  事务回滚: {e}")
        conn.rollback()
    else:
        conn.commit()
        print("  事务提交成功")

    # 验证：检查 posts 表
    cursor.execute("SELECT * FROM posts")
    posts = cursor.fetchall()
    print(f"  posts 表记录数: {len(posts)}")

    # 更新操作
    cursor.execute(
        "UPDATE users SET age = ? WHERE name = ?",
        (31, "Alice")
    )
    conn.commit()
    print(f"  更新 Alice 年龄为 31")

    # 删除操作
    cursor.execute("DELETE FROM users WHERE name = ?", ("Diana",))
    conn.commit()
    print(f"  删除 Diana")

    # 最终查询
    cursor.execute("SELECT * FROM users ORDER BY id")
    all_users = cursor.fetchall()
    print(f"  最终用户列表: {len(all_users)} 条")
    for user in all_users:
        print(f"    ID={user[0]}, {user[1]}, {user[2]} 岁")

    # 关闭连接
    conn.close()

    # 清理
    if os.path.exists(db_path):
        os.remove(db_path)
        print(f"\n[cleanup] 已删除数据库")

    print("\n=== PASS ===")

if __name__ == "__main__":
    main()
