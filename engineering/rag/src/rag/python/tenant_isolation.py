"""
多租户隔离模块

实现租户级别权限校验、配额管理、数据隔离。
"""

import logging
from typing import Optional

logger = logging.getLogger(__name__)


class QuotaExceededError(Exception):
    """配额超限异常"""
    pass


class QuotaStatus:
    """配额状态"""
    def __init__(self, used: int = 0, limit: int = 0):
        self.used = used
        self.limit = limit
        self.remaining = max(0, limit - used)


class TenantIsolation:
    """
    多租户隔离层

    提供租户级别权限校验和数据隔离。
    """

    def __init__(self, db_api=None):
        self.db_api = db_api

    def check_tenant_access(self, tenant_id: int, resource_id: int,
                            resource_type: str) -> bool:
        """
        检查租户是否有权访问资源

        Args:
            tenant_id: 租户 ID
            resource_id: 资源 ID
            resource_type: 资源类型 (document/chunk)

        Returns:
            有权限返回 True
        """
        # TODO: 实际查询资源所属租户
        return True

    def filter_results_by_tenant(self, tenant_id: int,
                                  results: list) -> list:
        """
        过滤检索结果，只返回当前租户的资源

        Args:
            tenant_id: 租户 ID
            results: 检索结果列表

        Returns:
            过滤后的结果列表
        """
        return [
            r for r in results
            if (getattr(r, "metadata", {}) or {}).get("tenant_id") == tenant_id
        ]

    @staticmethod
    def enforce_tenant_id(data: dict) -> dict:
        """
        强制 tenant_id 过滤

        确保所有数据都带有 tenant_id。

        Args:
            data: 数据字典

        Returns:
            确保包含 tenant_id 的数据
        """
        if "tenant_id" not in data:
            raise ValueError("tenant_id 是必填字段")
        return data


class TenantQuota:
    """
    租户配额管理

    管理文档、chunk、存储空间的配额。
    """

    DEFAULT_QUOTAS = {
        "documents": 1000,
        "chunks": 100000,
        "storage_gb": 50,
    }

    def __init__(self, redis_client=None):
        self.redis = redis_client

    def check_quota(self, tenant_id: int, resource: str) -> QuotaStatus:
        """
        检查租户配额

        Args:
            tenant_id: 租户 ID
            resource: 资源类型 (documents/chunks/storage_gb)

        Returns:
            QuotaStatus
        """
        limit = self.DEFAULT_QUOTAS.get(resource, 100)
        used = 0

        if self.redis:
            key = f"quota:{tenant_id}:{resource}"
            used = int(self.redis.get(key) or 0)

        return QuotaStatus(used=used, limit=limit)

    def increment_usage(self, tenant_id: int, resource: str, delta: int = 1):
        """
        增加租户使用量

        Args:
            tenant_id: 租户 ID
            resource: 资源类型
            delta: 增量
        """
        if self.redis:
            key = f"quota:{tenant_id}:{resource}"
            self.redis.incrby(key, delta)