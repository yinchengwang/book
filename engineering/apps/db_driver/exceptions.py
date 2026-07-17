"""
build_my_db 异常类定义

遵循 DB-API 2.0 规范
https://www.python.org/dev/peps/pep-0249/
"""

import sys


class Warning(Exception):
    """数据库警告（如数据截断）"""
    pass


class Error(Exception):
    """所有数据库异常的基类"""
    pass


class InterfaceError(Error):
    """数据库接口错误（如连接失败）"""
    pass


class DatabaseError(Error):
    """数据库错误（如查询执行失败）"""
    pass


class DataError(DatabaseError):
    """数据错误（如除零、超出范围）"""
    pass


class OperationalError(DatabaseError):
    """数据库操作错误（如连接断开、超时）"""
    pass


class IntegrityError(DatabaseError):
    """完整性约束错误（如主键冲突，外键引用失败）"""
    pass


class InternalError(DatabaseError):
    """数据库内部错误"""
    pass


class ProgrammingError(DatabaseError):
    """编程错误（如 SQL 语法错误、表不存在）"""
    pass


class NotSupportedError(DatabaseError):
    """不支持的操作"""
    pass


class PoolError(Error):
    """连接池错误"""
    pass


# Python 3.11+ 支持 ExceptionGroup
try:
    from exceptiongroup import ExceptionGroup
    _uses_exceptiongroup = True
except ImportError:
    _uses_exceptiongroup = False


def _check_closed(operation):
    """装饰器：检查连接或游标是否已关闭"""
    def wrapper(self, *args, **kwargs):
        if self._closed:
            raise ProgrammingError("Cannot operate on closed cursor/connection")
        return operation(self, *args, **kwargs)
    return wrapper


def _translate_error(returncode: int, stderr: str) -> Exception:
    """
    将 CLI 返回码和错误输出转换为适当的异常

    参数:
        returncode: CLI 进程返回码
        stderr: 标准错误输出

    返回:
        适当的异常实例
    """
    if returncode == 0:
        return None

    # 解析错误信息
    error_msg = stderr.strip()

    # 根据错误类型映射到对应异常
    if "语法错误" in error_msg or "syntax error" in error_msg.lower():
        return ProgrammingError(f"SQL syntax error: {error_msg}")

    if "不存在" in error_msg or "not found" in error_msg.lower():
        return ProgrammingError(f"Object not found: {error_msg}")

    if "主键" in error_msg or "primary key" in error_msg.lower():
        return IntegrityError(f"Primary key violation: {error_msg}")

    if "外键" in error_msg or "foreign key" in error_msg.lower():
        return IntegrityError(f"Foreign key violation: {error_msg}")

    if "唯一" in error_msg or "unique" in error_msg.lower():
        return IntegrityError(f"Unique constraint violation: {error_msg}")

    if "NOT NULL" in error_msg or "not null" in error_msg.lower():
        return IntegrityError(f"NOT NULL constraint violation: {error_msg}")

    if "超时" in error_msg or "timeout" in error_msg.lower():
        return OperationalError(f"Operation timeout: {error_msg}")

    if "连接" in error_msg or "connect" in error_msg.lower():
        return OperationalError(f"Connection error: {error_msg}")

    # 默认返回操作错误
    return OperationalError(f"Database error: {error_msg}")
