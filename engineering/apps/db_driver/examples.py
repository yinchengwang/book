"""
build_my_db Python 驱动使用示例

这个示例展示如何使用 Python 驱动来操作 Build My DB 数据库，
适合用于 RAG（检索增强生成）项目的向量存储。
"""

import sys
import os

# 添加当前目录到路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import build_my_db as db


def example_basic_operations():
    """基本操作示例"""
    print("\n" + "=" * 60)
    print("基本操作示例")
    print("=" * 60)

    # 连接数据库
    conn = db.connect("example.db")

    # 创建游标
    cursor = conn.cursor()

    # 创建表
    cursor.execute("""
        CREATE TABLE documents (
            id INT PRIMARY KEY,
            title TEXT,
            content TEXT,
            embedding BLOB,
            created_at TEXT
        )
    """)
    print("✓ 创建 documents 表")

    # 插入数据
    cursor.execute(
        "INSERT INTO documents VALUES (%s, %s, %s, %s, %s)",
        (1, "Python 教程", "Python 是一种高级编程语言...", None, "2024-01-01")
    )
    print("✓ 插入文档 1")

    cursor.execute(
        "INSERT INTO documents VALUES (%s, %s, %s, %s, %s)",
        (2, "数据库基础", "数据库是存储数据的容器...", None, "2024-01-02")
    )
    print("✓ 插入文档 2")

    # 查询数据
    cursor.execute("SELECT * FROM documents WHERE id = %s", (1,))
    row = cursor.fetchone()
    print(f"✓ 查询结果: {row}")

    # 使用 fetchall
    cursor.execute("SELECT * FROM documents")
    all_rows = cursor.fetchall()
    print(f"✓ 所有文档: {len(all_rows)} 条")

    # 更新数据
    cursor.execute(
        "UPDATE documents SET title = %s WHERE id = %s",
        ("Python 教程（更新版）", 1)
    )
    print("✓ 更新文档标题")

    # 删除数据
    cursor.execute("DELETE FROM documents WHERE id = %s", (2,))
    print("✓ 删除文档 2")

    # 提交事务
    conn.commit()

    # 关闭连接
    conn.close()
    print("✓ 关闭连接")

    # 清理
    if os.path.exists("example.db"):
        os.remove("example.db")


def example_rag_usage():
    """RAG 使用场景示例"""
    print("\n" + "=" * 60)
    print("RAG 使用场景示例")
    print("=" * 60)

    conn = db.connect("rag_demo.db")

    # 创建文档表（用于 RAG）
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS chunks (
            id INT PRIMARY KEY,
            document_id INT,
            chunk_text TEXT,
            embedding_id INT,
            page_num INT,
            created_at TEXT
        )
    """)
    print("✓ 创建 chunks 表")

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS embeddings (
            id INT PRIMARY KEY,
            chunk_id INT,
            vector_data BLOB,
            model TEXT,
            dimensions INT,
            created_at TEXT
        )
    """)
    print("✓ 创建 embeddings 表")

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS documents (
            id INT PRIMARY KEY,
            title TEXT,
            source TEXT,
            created_at TEXT
        )
    """)
    print("✓ 创建 documents 表")

    # 插入文档
    doc_data = [
        (1, "机器学习入门", "机器学习是人工智能的一个分支...", "2024-01-01"),
        (2, "深度学习基础", "深度学习使用多层神经网络...", "2024-01-02"),
        (3, "自然语言处理", "自然语言处理研究人机交互...", "2024-01-03"),
    ]

    cursor.executemany(
        "INSERT INTO documents VALUES (%s, %s, %s, %s)",
        doc_data
    )
    print(f"✓ 批量插入 {len(doc_data)} 个文档")

    # 插入文档块
    chunk_data = [
        (1, 1, "机器学习是人工智能的一个分支...", 1, 1, "2024-01-01"),
        (2, 1, "它使用数据来训练模型...", 2, 2, "2024-01-01"),
        (3, 2, "深度学习使用多层神经网络...", 3, 1, "2024-01-02"),
        (4, 2, "这些网络可以自动学习特征...", 4, 2, "2024-01-02"),
        (5, 3, "自然语言处理研究人机交互...", 5, 1, "2024-01-03"),
    ]

    cursor.executemany(
        "INSERT INTO chunks VALUES (%s, %s, %s, %s, %s, %s)",
        chunk_data
    )
    print(f"✓ 批量插入 {len(chunk_data)} 个文档块")

    # 模拟向量搜索（简化版）
    print("\n--- 模拟向量搜索 ---")

    # 1. 查询相关文档块
    cursor.execute("""
        SELECT c.id, c.chunk_text, d.title
        FROM chunks c
        JOIN documents d ON c.document_id = d.id
        WHERE c.chunk_text LIKE '%学习%'
    """)

    results = cursor.fetchall()
    print(f"找到 {len(results)} 个相关块:")
    for row in results:
        print(f"  - [块 {row[0]}] {row[1][:50]}... (来自: {row[2]})")

    # 2. 获取文档统计
    cursor.execute("""
        SELECT d.title, COUNT(c.id) as chunk_count
        FROM documents d
        LEFT JOIN chunks c ON d.id = c.document_id
        GROUP BY d.id
    """)

    stats = cursor.fetchall()
    print("\n文档统计:")
    for row in stats:
        print(f"  - {row[0]}: {row[1]} 个块")

    conn.commit()
    conn.close()

    # 清理
    if os.path.exists("rag_demo.db"):
        os.remove("rag_demo.db")


def example_transaction_control():
    """事务控制示例"""
    print("\n" + "=" * 60)
    print("事务控制示例")
    print("=" * 60)

    conn = db.connect("transaction_demo.db")
    cursor = conn.cursor()

    # 创建表
    cursor.execute("""
        CREATE TABLE accounts (
            id INT PRIMARY KEY,
            name TEXT,
            balance REAL
        )
    """)

    # 插入初始数据
    cursor.execute("INSERT INTO accounts VALUES (1, 'Alice', 1000)")
    cursor.execute("INSERT INTO accounts VALUES (2, 'Bob', 500)")
    conn.commit()
    print("✓ 初始化账户余额")

    # 转账操作（原子性）
    print("\n--- 执行转账 ---")

    try:
        # Alice 转给 Bob 100 元
        cursor.execute("UPDATE accounts SET balance = balance - 100 WHERE id = 1")
        print("✓ Alice 扣款 100")

        cursor.execute("UPDATE accounts SET balance = balance + 100 WHERE id = 2")
        print("✓ Bob 到账 100")

        conn.commit()
        print("✓ 事务提交")

    except Exception as e:
        conn.rollback()
        print(f"✗ 事务回滚: {e}")

    # 验证结果
    cursor.execute("SELECT * FROM accounts ORDER BY id")
    accounts = cursor.fetchall()
    print("\n最终余额:")
    for acc in accounts:
        print(f"  - {acc[1]}: {acc[2]}")

    conn.close()

    # 清理
    if os.path.exists("transaction_demo.db"):
        os.remove("transaction_demo.db")


def example_context_manager():
    """上下文管理器示例（推荐方式）"""
    print("\n" + "=" * 60)
    print("上下文管理器示例（推荐方式）")
    print("=" * 60)

    # 使用 with 语句自动管理连接
    with db.connect("context_demo.db") as conn:
        cursor = conn.cursor()

        cursor.execute("CREATE TABLE users (id INT, name TEXT)")

        # 批量插入
        users = [(i, f"User{i}") for i in range(1, 101)]
        cursor.executemany("INSERT INTO users VALUES (%s, %s)", users)
        print(f"✓ 插入 100 个用户")

        # 查询
        cursor.execute("SELECT COUNT(*) FROM users")
        count = cursor.fetchone()[0]
        print(f"✓ 用户总数: {count}")

        # 使用便捷方法
        rows = conn.query("SELECT * FROM users WHERE id > %s", (95,))
        print(f"✓ 便捷查询结果: {rows}")

    # 连接已自动关闭
    print("✓ 连接自动关闭")

    # 清理
    if os.path.exists("context_demo.db"):
        os.remove("context_demo.db")


def example_batch_processing():
    """批量处理示例"""
    print("\n" + "=" * 60)
    print("批量处理示例")
    print("=" * 60)

    conn = db.connect("batch_demo.db")
    cursor = conn.cursor()

    cursor.execute("CREATE TABLE logs (id INT, message TEXT, level TEXT)")

    # 模拟批量导入日志
    log_levels = ["INFO", "WARNING", "ERROR"]
    batch_size = 1000

    import random
    logs = []
    for i in range(batch_size):
        level = random.choice(log_levels)
        message = f"Log message {i}: Something happened..."
        logs.append((i, message, level))

    cursor.executemany(
        "INSERT INTO logs VALUES (%s, %s, %s)",
        logs
    )
    print(f"✓ 批量插入 {batch_size} 条日志")

    # 统计各级别日志数量
    for level in log_levels:
        cursor.execute("SELECT COUNT(*) FROM logs WHERE level = %s", (level,))
        count = cursor.fetchone()[0]
        print(f"  - {level}: {count} 条")

    conn.commit()
    conn.close()

    # 清理
    if os.path.exists("batch_demo.db"):
        os.remove("batch_demo.db")


def main():
    """运行所有示例"""
    print("\n" + "=" * 60)
    print("Build My DB Python 驱动示例")
    print("=" * 60)

    example_basic_operations()
    example_rag_usage()
    example_transaction_control()
    example_context_manager()
    example_batch_processing()

    print("\n" + "=" * 60)
    print("所有示例执行完成！")
    print("=" * 60 + "\n")


if __name__ == "__main__":
    main()