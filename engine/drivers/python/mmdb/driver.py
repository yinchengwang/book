"""MMDB Python Driver

换行分隔 JSON 协议（NDJSON）：
  请求:  {"sql": "...", "params": [...]}\\n
  响应:  {"ok": true,  "columns": [...], "rows": [[...]], "rowcount": N}\\n
         {"ok": false, "error": "..."}\\n
"""
import socket
import json


class MMDBError(Exception):
    """服务器返回的 SQL 执行错误。"""


class MMDBClient:
    def __init__(self, host: str, port: int = 8080, timeout: float = 10.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self._socket = None
        self._recv_buf = b""

    def connect(self):
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.settimeout(self.timeout)
        self._socket.connect((self.host, self.port))

    def close(self):
        if self._socket:
            self._socket.close()
            self._socket = None
        self._recv_buf = b""

    # ------------------------------------------------------------------
    # 协议层
    # ------------------------------------------------------------------

    def _recv_line(self) -> bytes:
        """读取一个 '\\n' 终止的行（处理粘包/半包）。"""
        while b"\n" not in self._recv_buf:
            chunk = self._socket.recv(65536)
            if not chunk:
                raise MMDBError("connection closed by server")
            self._recv_buf += chunk
        line, self._recv_buf = self._recv_buf.split(b"\n", 1)
        return line

    def _roundtrip(self, sql: str, params) -> dict:
        if self._socket is None:
            raise MMDBError("not connected")
        request = json.dumps({"sql": sql, "params": list(params or [])})
        self._socket.sendall(request.encode("utf-8") + b"\n")
        response = json.loads(self._recv_line().decode("utf-8"))
        if not response.get("ok"):
            raise MMDBError(response.get("error", "unknown server error"))
        return response

    # ------------------------------------------------------------------
    # DB-API 风格接口
    # ------------------------------------------------------------------

    def query(self, sql: str, params=None) -> list:
        """执行查询，返回行列表（每行为 list）。"""
        return self._roundtrip(sql, params).get("rows", [])

    def execute(self, sql: str, params=None) -> int:
        """执行非查询语句，返回受影响行数。"""
        return self._roundtrip(sql, params).get("rowcount", 0)

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        self.close()
