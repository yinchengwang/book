"""
build_my_db - Python DB-API 2.0 兼容驱动
类似 psycopg2，用于连接 Build My DB 数据库

使用方式:
    import build_my_db as db

    # 直接连接
    conn = db.connect("my.db")
    cur = conn.cursor()
    cur.execute("SELECT * FROM users WHERE id = %s", (1,))
    print(cur.fetchall())
    conn.close()

    # 使用连接池
    pool = db.get_pool("my.db")
    with pool.getconn() as conn:
        cur = conn.cursor()
        cur.execute("SELECT * FROM users")
        print(cur.fetchall())
"""

__version__ = "0.1.0"
__author__ = "Build My DB Team"

from .connection import Connection
from .cursor import Cursor
from .pool import (
    ConnectionPool,
    PooledConnection,
    get_pool,
    close_all_pools,
    LocalConnection,
)
from .cli import CLIExecutor
from .exceptions import (
    DatabaseError,
    Error,
    IntegrityError,
    InterfaceError,
    InternalError,
    NotSupportedError,
    OperationalError,
    ProgrammingError,
    Warning,
    PoolError,
)

# DB-API 2.0 必需常量
apilevel = "2.0"
threadsafety = 1  # 不支持多线程共享连接
paramstyle = "pyformat"  # 使用 %(name)s 或 %s 格式


def connect(database: str, **kwargs):
    """
    创建数据库连接

    参数:
        database: 数据库文件路径
        **kwargs: 其他可选参数
            - host: 主机地址（未来支持远程连接）
            - port: 端口号
            - timeout: 超时时间（秒）
            - cli_path: CLI 工具路径

    返回:
        Connection 对象
    """
    return Connection(database=database, **kwargs)


# 导出公共接口
__all__ = [
    # 核心函数
    "connect",

    # 连接池
    "ConnectionPool",
    "PooledConnection",
    "get_pool",
    "close_all_pools",
    "LocalConnection",

    # 异常类
    "Warning",
    "Error",
    "InterfaceError",
    "DatabaseError",
    "DataError",
    "OperationalError",
    "IntegrityError",
    "InternalError",
    "ProgrammingError",
    "NotSupportedError",
    "PoolError",

    # 常量
    "apilevel",
    "threadsafety",
    "paramstyle",

    # 类（用于类型标注）
    "Connection",
    "Cursor",
]
