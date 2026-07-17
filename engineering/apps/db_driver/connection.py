"""
Connection 类 - 管理数据库连接

遵循 DB-API 2.0 规范
"""

import os
from typing import Optional, Any
from .cursor import Cursor
from .cli import CLIExecutor
from .exceptions import (
    Warning, Error, InterfaceError, DatabaseError,
    OperationalError, ProgrammingError
)


class Connection:
    """
    数据库连接

    管理与数据库的连接，支持事务操作
    """

    def __init__(
        self,
        database: str,
        host: str = "localhost",
        port: int = 5432,
        user: str = None,
        password: str = None,
        timeout: int = 30,
        cli_path: str = None,
        autocommit: bool = False,
    ):
        """
        初始化连接

        参数:
            database: 数据库文件路径
            host: 主机地址（未来支持远程连接）
            port: 端口号
            user: 用户名（暂不支持）
            password: 密码（暂不支持）
            timeout: 命令超时时间（秒）
            cli_path: CLI 工具路径
            autocommit: 是否自动提交
        """
        self._database = database
        self._host = host
        self._port = port
        self._user = user
        self._password = password
        self._timeout = timeout
        self._cli_path = cli_path
        self._autocommit = autocommit

        # 连接状态
        self._closed = False
        self._in_transaction = False

        # 创建 CLI 执行器
        try:
            self._executor = CLIExecutor(
                database=database,
                cli_path=cli_path,
                timeout=timeout
            )
        except FileNotFoundError as e:
            raise InterfaceError(str(e))

        # 游标缓存
        self._cursors = []

    def __enter__(self) -> 'Connection':
        """支持 with 语句"""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """支持 with 语句的清理"""
        if exc_type is not None:
            self.rollback()
        self.close()

    def cursor(self) -> Cursor:
        """
        创建游标

        返回:
            Cursor 对象
        """
        self._check_closed()

        cursor = Cursor(self)
        self._cursors.append(cursor)
        return cursor

    def commit(self) -> None:
        """
        提交当前事务

        注意：CLI 工具目前每次执行后自动提交
        """
        self._check_closed()
        self._in_transaction = False

    def rollback(self) -> None:
        """
        回滚当前事务
        """
        self._check_closed()

        if self._in_transaction:
            try:
                self._executor.execute("ROLLBACK")
            except:
                pass
            self._in_transaction = False

    def close(self) -> None:
        """
        关闭连接

        关闭所有游标并关闭连接
        """
        if self._closed:
            return

        # 关闭所有游标
        for cursor in self._cursors:
            try:
                cursor.close()
            except:
                pass

        self._cursors = []
        self._closed = True

    @property
    def closed(self) -> bool:
        """返回连接是否已关闭"""
        return self._closed

    @property
    def in_transaction(self) -> bool:
        """返回是否在事务中"""
        return self._in_transaction

    def _check_closed(self) -> None:
        """检查连接是否已关闭"""
        if self._closed:
            raise ProgrammingError("Cannot operate on closed connection")

    # ========== 上下文管理器支持 ==========

    def __del__(self):
        """析构函数，确保关闭连接"""
        if not self._closed:
            try:
                self.close()
            except:
                pass

    # ========== 便捷方法 ==========

    def execute(self, sql: str, parameters: Optional[tuple] = None) -> Cursor:
        """
        执行 SQL 的便捷方法

        参数:
            sql: SQL 语句
            parameters: 参数

        返回:
            Cursor 对象
        """
        cursor = self.cursor()
        return cursor.execute(sql, parameters)

    def query(self, sql: str, parameters: Optional[tuple] = None) -> list:
        """
        执行查询并返回结果的便捷方法

        参数:
            sql: SQL 语句
            parameters: 参数

        返回:
            结果行列表
        """
        cursor = self.cursor()
        cursor.execute(sql, parameters)
        return cursor.fetchall()

    def get_tables(self) -> list:
        """获取所有表名"""
        try:
            cursor = self.cursor()
            cursor.execute("SHOW TABLES")
            return [row[0] for row in cursor.fetchall()]
        except:
            return []

    def get_schema(self, table_name: str) -> dict:
        """
        获取表结构

        返回:
            {"columns": [...], "types": [...], ...}
        """
        try:
            cursor = self.cursor()
            cursor.execute(f"DESCRIBE {table_name}")
            columns = cursor.fetchall()
            return {
                "name": table_name,
                "columns": [col[0] for col in columns],
                "types": [col[1] for col in columns],
            }
        except:
            return {}

    # ========== 事务控制 ==========

    def begin(self) -> None:
        """
        开始事务

        等同于 BEGIN 语句
        """
        self._check_closed()
        self._in_transaction = True

    def savepoint(self, name: str) -> None:
        """
        创建保存点

        参数:
            name: 保存点名称
        """
        self._check_closed()
        self._executor.execute(f"SAVEPOINT {name}")

    def rollback_to(self, name: str) -> None:
        """
        回滚到保存点

        参数:
            name: 保存点名称
        """
        self._check_closed()
        self._executor.execute(f"ROLLBACK TO SAVEPOINT {name}")

    def release_savepoint(self, name: str) -> None:
        """
        释放保存点

        参数:
            name: 保存点名称
        """
        self._check_closed()
        self._executor.execute(f"RELEASE SAVEPOINT {name}")