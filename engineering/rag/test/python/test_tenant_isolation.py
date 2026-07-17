"""
多租户隔离单元测试
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "rag", "python"))

from tenant_isolation import TenantIsolation, TenantQuota, QuotaExceededError, QuotaStatus


class MockResult:
    """模拟检索结果"""
    def __init__(self, chunk_id=0, metadata=None):
        self.chunk_id = chunk_id
        self.metadata = metadata or {}


class TestTenantIsolation:
    """TenantIsolation 单元测试"""

    def setup_method(self):
        self.isolation = TenantIsolation()

    def test_check_access_default(self):
        """默认通过"""
        result = self.isolation.check_tenant_access(1, 100, "document")
        assert result is True

    def test_filter_results_by_tenant(self):
        """按租户过滤"""
        results = [
            MockResult(1, {"tenant_id": 1}),
            MockResult(2, {"tenant_id": 2}),
            MockResult(3, {"tenant_id": 1}),
        ]
        filtered = self.isolation.filter_results_by_tenant(1, results)
        assert len(filtered) == 2

    def test_enforce_tenant_id(self):
        """强制 tenant_id"""
        data = self.isolation.enforce_tenant_id({"tenant_id": 1, "name": "test"})
        assert data["tenant_id"] == 1

    def test_enforce_tenant_id_missing(self):
        """缺少 tenant_id 时报错"""
        try:
            self.isolation.enforce_tenant_id({"name": "test"})
            assert False, "应该抛出异常"
        except ValueError:
            pass


class TestTenantQuota:
    """TenantQuota 单元测试"""

    def setup_method(self):
        self.quota = TenantQuota()

    def test_default_quota(self):
        """默认配额"""
        status = self.quota.check_quota(1, "documents")
        assert isinstance(status, QuotaStatus)
        assert status.limit == 1000

    def test_increment_usage(self):
        """增加使用量（无 Redis 时静默）"""
        # 不抛错即通过
        self.quota.increment_usage(1, "documents", 5)

    def test_storage_quota(self):
        """存储配额"""
        status = self.quota.check_quota(1, "storage_gb")
        assert status.limit == 50


class TestQuotaStatus:
    """QuotaStatus 测试"""

    def test_remaining_calculation(self):
        """剩余配额计算"""
        status = QuotaStatus(used=30, limit=100)
        assert status.remaining == 70

    def test_remaining_overflow(self):
        """超额时剩余为 0"""
        status = QuotaStatus(used=150, limit=100)
        assert status.remaining == 0

    def test_default_values(self):
        """默认值"""
        status = QuotaStatus()
        assert status.used == 0
        assert status.limit == 0


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])