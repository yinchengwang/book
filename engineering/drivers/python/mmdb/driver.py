import socket
import json

class MMDBClient:
    def __init__(self, host: str, port: int = 8080):
        self.host = host
        self.port = port
        self._socket = None

    def connect(self):
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.connect((self.host, self.port))

    def close(self):
        if self._socket:
            self._socket.close()

    def query(self, sql: str) -> list:
        # 发送请求并接收响应
        pass

    def execute(self, sql: str) -> int:
        pass

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        self.close()
