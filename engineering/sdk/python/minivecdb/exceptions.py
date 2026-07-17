"""
MiniVecDB 异常定义
"""
from typing import Optional


class MiniVecDBError(Exception):
    """基础异常类"""
    def __init__(self, message: str, code: Optional[str] = None):
        super().__init__(message)
        self.message = message
        self.code = code


class ConnectionError(MiniVecDBError):
    """连接错误"""
    pass


class TimeoutError(MiniVecDBError):
    """超时错误"""
    pass


class NotFoundError(MiniVecDBError):
    """未找到错误"""
    pass


class ValidationError(MiniVecDBError):
    """验证错误"""
    pass


class APIError(MiniVecDBError):
    """API 错误响应"""
    def __init__(self, message: str, code: str, status_code: int):
        super().__init__(message, code)
        self.status_code = status_code
