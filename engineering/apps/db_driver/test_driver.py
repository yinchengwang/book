"""
build_my_db 驱动测试
"""

import pytest
import os
import sys
import tempfile

# 添加父目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import build_my_db as db


class TestConnection:
    """连接测试"""

    @pytest.fixture
    def test_db(self):
        """创建临时测试数据库"""
        fd, path = tempfile.mkstemp(suffix=".db")
        os.close(fd)
        yield path
        if os.path.exists(path):
            os.remove(path)

    def test_connect(self, test_db):
        """测试连接创建"""
        conn = db.connect(test_db)
        assert conn is not None
        assert not conn.closed
        conn.close()
        assert conn.closed

    def test_connect_with_context(self, test_db):
        """测试上下文管理器"""
        with db.connect(test_db) as conn:
            assert not conn.closed
        # 退出 with 块后应该已关闭
        assert conn.closed

    def test_connect_not_found(self):
        """测试连接不存在的数据库"""
        # 应该能创建新数据库
        with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as f:
            path = f.name
        os.remove(path)

        conn = db.connect(path)
        assert conn is not None
        conn.close()

        # 清理
        if os.path.exists(path):
            os.remove(path)


class TestCursor:
    """游标测试"""

    @pytest.fixture
    def conn(self):
        """创建测试连接"""
        fd, path = tempfile.mkstemp(suffix=".db")
        os.close(fd)
        conn = db.connect(path)
        yield conn
        conn.close()
        if os.path.exists(path):
            os.remove(path)

    def test_cursor(self, conn):
        """测试游标创建"""
        cur = conn.cursor()
        assert cur is not None
        cur.close()

    def test_execute_create_table(self, conn):
        """测试创建表"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")
        assert cur.rowcount >= 0

    def test_execute_insert(self, conn):
        """测试插入数据"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")
        cur.execute("INSERT INTO users VALUES (1, 'Alice')")
        assert cur.rowcount >= 0

    def test_execute_select(self, conn):
        """测试查询"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")
        cur.execute("INSERT INTO users VALUES (1, 'Alice')")
        cur.execute("INSERT INTO users VALUES (2, 'Bob')")
        cur.execute("SELECT * FROM users")

        rows = cur.fetchall()
        assert len(rows) == 2

    def test_execute_with_params(self, conn):
        """测试参数化查询"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")
        cur.execute("INSERT INTO users VALUES (1, 'Alice')")
        cur.execute("INSERT INTO users VALUES (2, 'Bob')")
        cur.execute("SELECT * FROM users WHERE id = %s", (1,))

        rows = cur.fetchall()
        assert len(rows) == 1
        assert rows[0][0] == 1

    def test_fetchone(self, conn):
        """测试 fetchone"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")
        cur.execute("INSERT INTO users VALUES (1, 'Alice')")
        cur.execute("INSERT INTO users VALUES (2, 'Bob')")
        cur.execute("SELECT * FROM users")

        assert cur.fetchone() == (1, 'Alice')
        assert cur.fetchone() == (2, 'Bob')
        assert cur.fetchone() is None

    def test_fetchmany(self, conn):
        """测试 fetchmany"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")
        for i in range(10):
            cur.execute(f"INSERT INTO users VALUES ({i}, 'User{i}')")

        cur.execute("SELECT * FROM users")
        cur.arraysize = 3

        rows = cur.fetchmany()
        assert len(rows) == 3

        rows = cur.fetchmany(2)
        assert len(rows) == 2

    def test_fetchall(self, conn):
        """测试 fetchall"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")
        for i in range(5):
            cur.execute(f"INSERT INTO users VALUES ({i}, 'User{i}')")

        cur.execute("SELECT * FROM users")
        rows = cur.fetchall()
        assert len(rows) == 5

    def test_executemany(self, conn):
        """测试 executemany"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")

        params = [(1, 'Alice'), (2, 'Bob'), (3, 'Charlie')]
        cur.executemany("INSERT INTO users VALUES (%s, %s)", params)

        cur.execute("SELECT COUNT(*) FROM users")
        count = cur.fetchone()[0]
        assert count == 3

    def test_iterator(self, conn):
        """测试迭代器协议"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")
        for i in range(3):
            cur.execute(f"INSERT INTO users VALUES ({i}, 'User{i}')")

        cur.execute("SELECT * FROM users")
        rows = list(cur)
        assert len(rows) == 3

    def test_description(self, conn):
        """测试 description 属性"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")
        cur.execute("SELECT id, name FROM users")

        assert cur.description is not None
        assert len(cur.description) == 2
        assert cur.description[0][0] == "id"
        assert cur.description[1][0] == "name"

    def test_rowcount(self, conn):
        """测试 rowcount 属性"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")
        cur.execute("INSERT INTO users VALUES (1, 'Alice')")

        cur.execute("SELECT * FROM users")
        assert cur.rowcount == 1


class TestTransaction:
    """事务测试"""

    @pytest.fixture
    def conn(self):
        """创建测试连接"""
        fd, path = tempfile.mkstemp(suffix=".db")
        os.close(fd)
        conn = db.connect(path)
        yield conn
        conn.close()
        if os.path.exists(path):
            os.remove(path)

    def test_commit(self, conn):
        """测试提交"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")
        conn.commit()

        # 数据应该存在
        cur.execute("SELECT * FROM users")
        assert cur.fetchall() == []

    def test_rollback(self, conn):
        """测试回滚"""
        cur = conn.cursor()
        cur.execute("CREATE TABLE users (id INT, name TEXT)")
        cur.execute("INSERT INTO users VALUES (1, 'Initial')")
        conn.commit()

        # 开始新事务
        cur.execute("UPDATE users SET name = 'Modified' WHERE id = 1")
        conn.rollback()

        # 数据应该恢复
        cur.execute("SELECT * FROM users")
        rows = cur.fetchall()
        assert len(rows) == 1


class TestConvenienceMethods:
    """便捷方法测试"""

    @pytest.fixture
    def conn(self):
        """创建测试连接"""
        fd, path = tempfile.mkstemp(suffix=".db")
        os.close(fd)
        conn = db.connect(path)
        yield conn
        conn.close()
        if os.path.exists(path):
            os.remove(path)

    def test_execute_method(self, conn):
        """测试 conn.execute()"""
        conn.execute("CREATE TABLE users (id INT, name TEXT)")
        conn.execute("INSERT INTO users VALUES (1, 'Alice')")

        cur = conn.execute("SELECT * FROM users")
        assert cur.fetchall() == [(1, 'Alice')]

    def test_query_method(self, conn):
        """测试 conn.query()"""
        conn.execute("CREATE TABLE users (id INT, name TEXT)")
        conn.execute("INSERT INTO users VALUES (1, 'Alice')")

        rows = conn.query("SELECT * FROM users WHERE id = %s", (1,))
        assert rows == [(1, 'Alice')]


class TestErrorHandling:
    """错误处理测试"""

    @pytest.fixture
    def conn(self):
        """创建测试连接"""
        fd, path = tempfile.mkstemp(suffix=".db")
        os.close(fd)
        conn = db.connect(path)
        yield conn
        conn.close()
        if os.path.exists(path):
            os.remove(path)

    def test_closed_connection(self, conn):
        """测试已关闭连接"""
        conn.close()
        with pytest.raises(db.ProgrammingError):
            conn.cursor()

    def test_closed_cursor(self, conn):
        """测试已关闭游标"""
        cur = conn.cursor()
        cur.close()
        with pytest.raises(db.ProgrammingError):
            cur.execute("SELECT 1")

    def test_sql_error(self, conn):
        """测试 SQL 错误"""
        cur = conn.cursor()
        with pytest.raises((db.ProgrammingError, db.OperationalError)):
            cur.execute("INVALID SQL SYNTAX")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])