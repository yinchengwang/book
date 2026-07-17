"""
连接池 - 管理多个数据库连接的池化

参考 psycopg2 的 connection pool 实现
"""

import threading
import queue
import time
from typing import Optional, Dict, Any, List
from .connection import Connection
from .exceptions import PoolError


class PooledConnection:
    """连接池中的连接包装器"""

    def __init__(self, conn: Connection, pool: 'ConnectionPool'):
        self.conn = conn
        self.pool = pool
        self.closed = False

    def cursor(self):
        """创建游标"""
        return self.conn.cursor()

    def commit(self):
        """提交事务"""
        self.conn.commit()

    def rollback(self):
        """回滚事务"""
        self.conn.rollback()

    def close(self):
        """归还连接到池"""
        if not self.closed:
            self.closed = True
            self.pool._return_connection(self)

    def execute(self, sql: str, parameters: Optional[tuple] = None):
        """执行 SQL"""
        return self.conn.execute(sql, parameters)

    def query(self, sql: str, parameters: Optional[tuple] = None) -> list:
        """执行查询"""
        return self.conn.query(sql, parameters)

    @property
    def closed(self) -> bool:
        return self._closed

    @closed.setter
    def closed(self, value: bool):
        self._closed = value


class ConnectionPool:
    """
    连接池

    管理一组数据库连接，支持连接复用和自动回收
    """

    def __init__(
        self,
        database: str,
        minconn: int = 1,
        maxconn: int = 10,
        timeout: int = 30,
        **kwargs
    ):
        """
        初始化连接池

        参数:
            database: 数据库文件路径
            minconn: 最小连接数
            maxconn: 最大连接数
            timeout: 获取连接超时时间（秒）
            **kwargs: 传递给 Connection 的其他参数
        """
        self.database = database
        self.minconn = minconn
        self.maxconn = maxconn
        self.timeout = timeout
        self.kwargs = kwargs

        self._pool: queue.Queue = queue.Queue(maxconn)
        self._size = 0
        self._lock = threading.Lock()
        self._initialized = False

    def _create_connection(self) -> Connection:
        """创建新连接"""
        return Connection(database=self.database, **self.kwargs)

    def _initialize(self):
        """初始化连接池（创建最小连接数）"""
        if self._initialized:
            return

        with self._lock:
            if self._initialized:
                return

            for _ in range(self.minconn):
                try:
                    conn = self._create_connection()
                    self._pool.put(conn)
                    self._size += 1
                except Exception as e:
                    # 连接失败，继续尝试创建其他连接
                    pass

            self._initialized = True

    def getconn(self, timeout: Optional[float] = None) -> PooledConnection:
        """
        从池中获取连接

        参数:
            timeout: 超时时间（秒），None 使用默认值

        返回:
            PooledConnection 对象

        异常:
            PoolError: 获取连接超时
        """
        self._initialize()

        if timeout is None:
            timeout = self.timeout

        try:
            conn = self._pool.get(timeout=timeout)
            return PooledConnection(conn, self)
        except queue.Empty:
            # 检查是否可以创建新连接
            with self._lock:
                if self._size < self.maxconn:
                    conn = self._create_connection()
                    self._size += 1
                    return PooledConnection(conn, self)

            raise PoolError(f"获取连接超时（{timeout}秒）")

    def _return_connection(self, pooled_conn: PooledConnection):
        """归还连接到池"""
        if pooled_conn.closed:
            return

        try:
            # 重置连接状态
            try:
                pooled_conn.conn.rollback()
            except:
                pass

            # 放回池中
            self._pool.put(pooled_conn.conn, block=False)
        except queue.Full:
            # 池已满，关闭连接
            with self._lock:
                self._size -= 1
            try:
                pooled_conn.conn.close()
            except:
                pass

    def putconn(self, conn: PooledConnection, close: bool = False):
        """
        归还连接到池

        参数:
            conn: PooledConnection 对象
            close: 是否关闭连接而不是放回池中
        """
        if close:
            with self._lock:
                self._size -= 1
            try:
                conn.conn.close()
            except:
                pass
        else:
            self._return_connection(conn)

    def closeall(self):
        """关闭池中所有连接"""
        with self._lock:
            while True:
                try:
                    conn = self._pool.get_nowait()
                    try:
                        conn.close()
                    except:
                        pass
                    self._size -= 1
                except queue.Empty:
                    break

            self._initialized = False

    def __enter__(self) -> 'ConnectionPool':
        """支持 with 语句"""
        self._initialize()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """退出 with 语句时关闭所有连接"""
        self.closeall()

    @property
    def size(self) -> int:
        """当前连接数"""
        with self._lock:
            return self._size

    @property
    def available(self) -> int:
        """可用连接数"""
        return self._pool.qsize()


# 全局连接池缓存
_pools: Dict[str, ConnectionPool] = {}
_pools_lock = threading.Lock()


def get_pool(
    database: str,
    minconn: int = 1,
    maxconn: int = 10,
    **kwargs
) -> ConnectionPool:
    """
    获取全局连接池

    参数:
        database: 数据库文件路径
        minconn: 最小连接数
        maxconn: 最大连接数
        **kwargs: 其他参数

    返回:
        ConnectionPool 对象
    """
    key = database

    with _pools_lock:
        if key not in _pools:
            _pools[key] = ConnectionPool(
                database=database,
                minconn=minconn,
                maxconn=maxconn,
                **kwargs
            )
        return _pools[key]


def close_all_pools():
    """关闭所有全局连接池"""
    with _pools_lock:
        for pool in _pools.values():
            pool.closeall()
        _pools.clear()


# Thread-local 存储当前连接
_local = threading.local()


def get_local_connection() -> Optional[PooledConnection]:
    """获取当前线程的本地连接"""
    return getattr(_local, 'connection', None)


def set_local_connection(conn: Optional[PooledConnection]):
    """设置当前线程的本地连接"""
    _local.connection = conn


class LocalConnection:
    """
    线程本地连接管理器

    为每个线程维护独立的数据库连接
    """

    def __init__(self, pool: ConnectionPool):
        self.pool = pool
        self._conn: Optional[PooledConnection] = None

    def __enter__(self) -> PooledConnection:
        """获取连接"""
        if self._conn is None or self._conn.closed:
            self._conn = self.pool.getconn()
        return self._conn

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """归还连接（不断开）"""
        if exc_type is not None:
            try:
                self._conn.rollback()
            except:
                pass
        self._conn = None