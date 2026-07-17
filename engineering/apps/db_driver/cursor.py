"""
Cursor 类 - 执行 SQL 并管理结果集

遵循 DB-API 2.0 规范
"""

from typing import Any, List, Optional, Tuple, Union
from .cli import CLIExecutor
from .exceptions import (
    Warning, Error, InterfaceError, DatabaseError,
    DataError, OperationalError, IntegrityError,
    InternalError, ProgrammingError, NotSupportedError
)


class Cursor:
    """
    数据库游标

    用于执行 SQL 语句并获取结果
    """

    # 类属性：描述信息
    description = None  # 列信息 (name, type_code, display_size, internal_size, precision, scale, null_ok)
    arraysize = 1      # fetchmany 返回的行数

    def __init__(self, connection: 'Connection'):
        """
        初始化游标

        参数:
            connection: 父连接对象
        """
        self._connection = connection
        self._executor = connection._executor
        self._closed = False

        # 结果集
        self._columns = None
        self._rows = None
        self._rowcount = -1
        self._current_row = 0

        # 最后执行的 SQL（用于调试）
        self._last_sql = None

    @property
    def description(self) -> Optional[List[Tuple]]:
        """
        返回结果集描述信息

        格式: [(name, type_code, display_size, internal_size, precision, scale, null_ok), ...]
        """
        if self._columns is None:
            return None

        return [
            (col, None, None, None, None, None, True)
            for col in self._columns
        ]

    @property
    def rowcount(self) -> int:
        """返回影响的行数"""
        return self._rowcount

    def close(self) -> None:
        """关闭游标"""
        self._closed = True
        self._columns = None
        self._rows = None

    def execute(self, operation: str, parameters: Optional[Tuple] = None) -> 'Cursor':
        """
        执行单条 SQL 语句

        参数:
            operation: SQL 语句
            parameters: 参数元组（用于占位符替换）

        返回:
            self（支持链式调用）
        """
        self._check_closed()
        self._last_sql = operation

        # 参数替换
        if parameters:
            sql = self._format_sql(operation, parameters)
        else:
            sql = operation

        # 执行 SQL
        try:
            self._columns, self._rows = self._executor.execute(sql)
            self._rowcount = len(self._rows) if self._rows else 0
            self._current_row = 0

        except Exception as e:
            # 重新抛出为数据库异常
            if isinstance(e, (DatabaseError, ProgrammingError, OperationalError)):
                raise
            raise InterfaceError(f"Failed to execute query: {e}")

        return self

    def executemany(self, operation: str, seq_of_parameters: List[Tuple]) -> 'Cursor':
        """
        执行多条 SQL 语句

        参数:
            operation: SQL 语句模板
            seq_of_parameters: 参数列表

        返回:
            self
        """
        self._check_closed()
        self._last_sql = operation

        total_rows = 0
        for params in seq_of_parameters:
            if params:
                sql = self._format_sql(operation, params)
            else:
                sql = operation

            try:
                self._executor.execute(sql)
                total_rows += 1
            except Exception as e:
                if isinstance(e, (DatabaseError, ProgrammingError, OperationalError)):
                    raise
                raise InterfaceError(f"Failed to execute query: {e}")

        self._rowcount = total_rows
        self._columns = None
        self._rows = None

        return self

    def fetchone(self) -> Optional[Tuple]:
        """
        获取下一行

        返回:
            下一行数据，或 None（没有更多数据）
        """
        self._check_closed()

        if self._rows is None:
            return None

        if self._current_row >= len(self._rows):
            return None

        row = self._rows[self._current_row]
        self._current_row += 1
        return tuple(row) if row else None

    def fetchmany(self, size: Optional[int] = None) -> List[Tuple]:
        """
        获取多行

        参数:
            size: 返回行数，默认使用 arraysize

        返回:
            行列表
        """
        self._check_closed()

        if self._rows is None:
            return []

        if size is None:
            size = self.arraysize

        rows = []
        for _ in range(size):
            if self._current_row >= len(self._rows):
                break
            row = self._rows[self._current_row]
            rows.append(tuple(row) if row else None)
            self._current_row += 1

        return rows

    def fetchall(self) -> List[Tuple]:
        """
        获取所有剩余行

        返回:
            所有行列表
        """
        self._check_closed()

        if self._rows is None:
            return []

        rows = []
        while self._current_row < len(self._rows):
            row = self._rows[self._current_row]
            rows.append(tuple(row) if row else None)
            self._current_row += 1

        return rows

    def __iter__(self):
        """支持迭代器协议"""
        self._check_closed()
        return self

    def __next__(self) -> Tuple:
        """迭代器下一步"""
        row = self.fetchone()
        if row is None:
            raise StopIteration
        return row

    def _format_sql(self, sql: str, parameters: Tuple) -> str:
        """
        格式化 SQL 语句，替换占位符

        支持的格式:
        - %s: 位置参数 (psycopg2 兼容)
        - %(name)s: 命名参数
        """
        if not parameters:
            return sql

        # 处理不同类型的参数
        formatted_params = []
        for p in parameters:
            if p is None:
                formatted_params.append("NULL")
            elif isinstance(p, bool):
                formatted_params.append("TRUE" if p else "FALSE")
            elif isinstance(p, (int, float)):
                formatted_params.append(str(p))
            elif isinstance(p, str):
                # 转义单引号
                escaped = p.replace("'", "''")
                formatted_params.append(f"'{escaped}'")
            elif isinstance(p, (list, tuple)):
                # 处理 IN 子句，如 (1, 2, 3) -> ('1', '2', '3')
                inner = ", ".join(self._format_value(v) for v in p)
                formatted_params.append(f"({inner})")
            else:
                formatted_params.append(self._format_value(p))

        # 替换占位符
        try:
            if "%s" in sql:
                return sql % tuple(formatted_params)
            elif "%(" in sql:
                # 命名参数
                if isinstance(parameters, dict):
                    return sql % parameters
                else:
                    return sql
            else:
                return sql
        except (TypeError, ValueError) as e:
            raise ProgrammingError(f"Error formatting SQL: {e}")

    def _format_value(self, value: Any) -> str:
        """格式化单个值"""
        if value is None:
            return "NULL"
        elif isinstance(value, bool):
            return "TRUE" if value else "FALSE"
        elif isinstance(value, (int, float)):
            return str(value)
        elif isinstance(value, str):
            escaped = value.replace("'", "''")
            return f"'{escaped}'"
        else:
            escaped = str(value).replace("'", "''")
            return f"'{escaped}'"

    def _check_closed(self) -> None:
        """检查游标是否已关闭"""
        if self._closed:
            raise ProgrammingError("Cannot operate on closed cursor")

    @property
    def connection(self) -> 'Connection':
        """返回创建此游标的连接（如果有）"""
        return self._connection

    @property
    def last_sql(self) -> Optional[str]:
        """返回最后执行的 SQL 语句"""
        return self._last_sql