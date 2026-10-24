"""MMDB Python 驱动集成测试（C9-3 Task #2）

测试策略：进程内起一个 Mock MMDB 服务器，后端为内存 SQLite
（sqlite3.connect(":memory:")），通过真实 TCP socket 走完
NDJSON 协议全链路 —— 无需部署真实 MMDB 服务即可做集成测试。

运行：cd drivers/python && python -m unittest test_driver -v
"""
import json
import sqlite3
import socket
import threading
import unittest

from mmdb.driver import MMDBClient, MMDBError


class MockMMDBServer:
    """基于内存 SQLite 的 Mock MMDB 服务器（NDJSON 协议）。"""

    def __init__(self):
        # check_same_thread=False：服务线程与测试线程共享一个内存库
        self._db = sqlite3.connect(":memory:", check_same_thread=False)
        self._db_lock = threading.Lock()
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind(("127.0.0.1", 0))  #  ephemeral 端口，避免冲突
        self._sock.listen(1)
        self.port = self._sock.getsockname()[1]
        self._thread = None
        self._running = False

    def start(self):
        self._running = True
        self._thread = threading.Thread(target=self._serve, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        try:
            # 触发 accept 退出
            with socket.create_connection(("127.0.0.1", self.port), timeout=2):
                pass
        except OSError:
            pass
        if self._thread:
            self._thread.join(timeout=5)
        self._sock.close()
        self._db.close()

    def _serve(self):
        while self._running:
            try:
                conn, _ = self._sock.accept()
            except OSError:
                break
            threading.Thread(target=self._handle, args=(conn,), daemon=True).start()

    def _handle(self, conn: socket.socket):
        buf = b""
        with conn:
            while self._running:
                try:
                    chunk = conn.recv(65536)
                except OSError:
                    break
                if not chunk:
                    break
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    response = self._execute(json.loads(line.decode("utf-8")))
                    conn.sendall(json.dumps(response).encode("utf-8") + b"\n")

    def _execute(self, request: dict) -> dict:
        sql = request["sql"]
        params = request.get("params", [])
        try:
            with self._db_lock:
                cur = self._db.execute(sql, params)
                if cur.description:  # SELECT / PRAGMA 等有结果集
                    columns = [d[0] for d in cur.description]
                    rows = [list(r) for r in cur.fetchall()]
                    return {"ok": True, "columns": columns,
                            "rows": rows, "rowcount": len(rows)}
                self._db.commit()
                return {"ok": True, "columns": [], "rows": [],
                        "rowcount": max(cur.rowcount, 0)}
        except sqlite3.Error as e:
            return {"ok": False, "error": str(e)}


class MMDBDriverIntegrationTest(unittest.TestCase):
    """驱动 <-> 内存 SQLite 端到端集成测试。"""

    @classmethod
    def setUpClass(cls):
        cls.server = MockMMDBServer()
        cls.server.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.stop()

    def setUp(self):
        self.client = MMDBClient("127.0.0.1", self.server.port)
        self.client.connect()
        # 每个用例一张干净的表
        self.client.execute("DROP TABLE IF EXISTS users")

    def tearDown(self):
        self.client.close()

    # --------------------------------------------------------------

    def test_connect_and_close(self):
        client = MMDBClient("127.0.0.1", self.server.port)
        client.connect()
        self.assertIsNotNone(client._socket)
        client.close()
        self.assertIsNone(client._socket)

    def test_execute_ddl(self):
        rc = self.client.execute(
            "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)")
        self.assertEqual(rc, 0)  # DDL 不影响行

    def test_execute_insert_rowcount(self):
        self.client.execute(
            "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)")
        self.assertEqual(
            self.client.execute("INSERT INTO users (name, age) VALUES (?, ?)",
                                ["alice", 30]), 1)
        self.assertEqual(
            self.client.execute("INSERT INTO users (name, age) VALUES (?, ?)",
                                ["bob", 25]), 1)

    def test_query_returns_rows(self):
        self.client.execute(
            "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)")
        self.client.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["alice", 30])
        self.client.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["bob", 25])

        rows = self.client.query("SELECT name, age FROM users ORDER BY age DESC")
        self.assertEqual(rows, [["alice", 30], ["bob", 25]])

    def test_query_with_params(self):
        self.client.execute(
            "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)")
        self.client.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["alice", 30])
        self.client.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["bob", 25])

        rows = self.client.query("SELECT name FROM users WHERE age > ?", [26])
        self.assertEqual(rows, [["alice"]])

    def test_error_propagates(self):
        with self.assertRaises(MMDBError):
            self.client.query("SELECT * FROM no_such_table")
        # 连接在错误后仍可用
        self.client.execute(
            "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)")
        self.client.execute("INSERT INTO users (name) VALUES (?)", ["carol"])
        self.assertEqual(self.client.query("SELECT name FROM users"), [["carol"]])

    def test_context_manager(self):
        with MMDBClient("127.0.0.1", self.server.port) as client:
            client.execute("CREATE TABLE ctx_t (v INTEGER)")
            client.execute("INSERT INTO ctx_t (v) VALUES (?)", [42])
            self.assertEqual(client.query("SELECT v FROM ctx_t"), [[42]])
            client.execute("DROP TABLE ctx_t")
        self.assertIsNone(client._socket)

    def test_unicode_roundtrip(self):
        self.client.execute(
            "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)")
        self.client.execute("INSERT INTO users (name) VALUES (?)", ["张三"])
        self.assertEqual(self.client.query("SELECT name FROM users"), [["张三"]])


if __name__ == "__main__":
    unittest.main()
