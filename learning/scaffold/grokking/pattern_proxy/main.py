"""
代理模式 (Proxy Pattern) 示例

为另一个对象提供替身或占位符以控制访问。
本示例展示: 虚代理、保护代理、缓存代理、远程代理。
"""

import abc
import time
import hashlib
import threading
from dataclasses import dataclass, field
from typing import Any


# =================================================================
# 公共接口
# =================================================================

class Document(abc.ABC):
    """文档接口, 所有代理和真实对象都实现此接口"""

    @abc.abstractmethod
    def display(self) -> str:
        """显示文档内容"""
        ...

    @abc.abstractmethod
    def get_size(self) -> int:
        """获取文档大小"""
        ...

    @abc.abstractmethod
    def get_metadata(self) -> dict:
        """获取文档元数据"""
        ...


# =================================================================
# 真实主题: 大文档
# =================================================================

class LargeDocument(Document):
    """真实文档, 加载开销大"""

    def __init__(self, filepath: str):
        self._filepath = filepath
        self._content: str | None = None
        self._size: int = 0
        self._metadata: dict = {}

        # 模拟昂贵的加载过程
        self._load_from_disk()

    def _load_from_disk(self) -> None:
        """模拟从磁盘加载 (耗时操作)"""
        print(f"  [LargeDocument] 正在从磁盘加载: {self._filepath} ...")
        time.sleep(1.5)  # 模拟 I/O 延迟

        # 模拟文件内容
        self._content = (
            f"这是文件 {self._filepath} 的内容。\n"
            + "数据行 " * 100
        )
        self._size = len(self._content.encode("utf-8"))
        self._metadata = {
            "path": self._filepath,
            "size_bytes": self._size,
            "lines": self._content.count("\n") + 1,
            "checksum": hashlib.md5(self._content.encode()).hexdigest(),
        }
        print(f"  [LargeDocument] 加载完成: {self._size} bytes")

    def display(self) -> str:
        print(f"  [LargeDocument] 显示内容 (前 80 字符)")
        return self._content[:80] + "..."

    def get_size(self) -> int:
        return self._size

    def get_metadata(self) -> dict:
        return self._metadata


# =================================================================
# 虚代理 (Virtual Proxy): 延迟加载
# =================================================================

class VirtualProxy(Document):
    """虚代理: 只有真正需要时才创建真实对象"""

    def __init__(self, filepath: str):
        self._filepath = filepath
        self._real_doc: LargeDocument | None = None

    def _get_real(self) -> LargeDocument:
        """延迟加载真实对象"""
        if self._real_doc is None:
            self._real_doc = LargeDocument(self._filepath)
        return self._real_doc

    def display(self) -> str:
        return self._get_real().display()

    def get_size(self) -> int:
        # 在获取元数据后再加载真实对象
        return self._get_real().get_size()

    def get_metadata(self) -> dict:
        return self._get_real().get_metadata()


# =================================================================
# 保护代理 (Protection Proxy): 权限控制
# =================================================================

class User:
    """用户角色"""
    def __init__(self, name: str, role: str):
        self.name = name
        self.role = role     # "admin", "editor", "viewer"

    def __repr__(self) -> str:
        return f"User({self.name}, {self.role})"


class ProtectedDocument(Document):
    """带权限控制的文档包装"""

    def __init__(self, real_doc: Document, owner: User):
        self._real = real_doc
        self._owner = owner
        self._acl: dict[str, set[str]] = {
            "view": {"admin", "editor", "viewer"},
            "edit": {"admin", "editor"},
            "delete": {"admin"},
        }

    def _check_access(self, operation: str, user: User) -> bool:
        allowed = self._acl.get(operation, set())
        return user.role in allowed

    def display(self, user: User | None = None) -> str:
        if user is None:
            return self._real.display()
        if not self._check_access("view", user):
            return f"  [保护代理] 权限不足: {user.name} 无权查看此文档"
        return self._real.display()

    def get_size(self) -> int:
        return self._real.get_size()

    def get_metadata(self) -> dict:
        return self._real.get_metadata()


# =================================================================
# 缓存代理 (Cache Proxy): 减少重复计算
# =================================================================

class CacheProxy(Document):
    """
    缓存代理: 缓存昂贵操作的结果。
    同时展示: 缓存失效 / TTL / 并发安全
    """

    def __init__(self, real_doc: Document, ttl_seconds: float = 5.0):
        self._real = real_doc
        self._ttl = ttl_seconds
        self._cache: dict[str, tuple[Any, float]] = {}
        self._lock = threading.Lock()

    def _cached_or_compute(self, key: str, compute_fn) -> Any:
        with self._lock:
            now = time.time()
            if key in self._cache:
                value, expire_at = self._cache[key]
                if now < expire_at:
                    print(f"  [缓存代理] 命中缓存: {key}")
                    return value
                else:
                    print(f"  [缓存代理] 缓存过期: {key}")
            # 计算新值
            value = compute_fn()
            self._cache[key] = (value, now + self._ttl)
            return value

    def display(self) -> str:
        return self._cached_or_compute("display", self._real.display)

    def get_size(self) -> int:
        return self._cached_or_compute("size", self._real.get_size)

    def get_metadata(self) -> dict:
        return self._cached_or_compute("metadata", self._real.get_metadata)

    def invalidate(self, key: str | None = None) -> None:
        with self._lock:
            if key is None:
                self._cache.clear()
                print("  [缓存代理] 清空所有缓存")
            elif key in self._cache:
                del self._cache[key]
                print(f"  [缓存代理] 失效缓存: {key}")


# =================================================================
# 远程代理 (Remote Proxy): 模拟 RPC 调用
# =================================================================

@dataclass
class RemoteResponse:
    status: int
    body: str
    latency_ms: float


class WeatherService:
    """远程天气服务 (模拟)"""

    def fetch_weather(self, city: str) -> RemoteResponse:
        # 模拟网络延迟
        time.sleep(0.3)
        return RemoteResponse(
            status=200,
            body=f'{{"city": "{city}", "temp": 25, "humidity": "60%"}}',
            latency_ms=300.0,
        )


class WeatherRemoteProxy:
    """
    远程代理: 隐藏远程调用细节。
    封装: 序列化、网络传输、重试、超时
    """

    def __init__(self):
        self._service = WeatherService()
        self._timeout_s = 2.0
        self._retries = 2

    def get_weather(self, city: str) -> dict:
        print(f"  [远程代理] 请求天气: {city} (超时={self._timeout_s}s, 重试={self._retries}次)")

        for attempt in range(1, self._retries + 2):
            try:
                resp = self._service.fetch_weather(city)
                print(f"    尝试 #{attempt}: 状态={resp.status}, 延迟={resp.latency_ms}ms")
                return {
                    "city": city,
                    "temperature": 25,
                    "humidity": "60%",
                    "source": "remote_proxy",
                }
            except Exception as e:
                print(f"    尝试 #{attempt} 失败: {e}")
                if attempt > self._retries:
                    raise
                time.sleep(0.1)

        return {}  # unreachable

    # 远程代理可以添加本地没有的功能
    def get_forecast(self, city: str, days: int) -> list[dict]:
        print(f"  [远程代理] 请求 {city} 未来 {days} 天预报")
        forecasts = []
        for d in range(days):
            resp = self._service.fetch_weather(f"{city}_day{d+1}")
            forecasts.append({"day": d + 1, "temp": 25 + d, "condition": "晴"})
        return forecasts


# =================================================================
# 主函数
# =================================================================

def main():
    print("=" * 60)
    print("代理模式 演示")
    print("=" * 60)

    # ── 1. 虚代理: 延迟加载 ──
    print("\n--- 1. 虚代理 (Virtual Proxy): 延迟加载大文档 ---")
    print("  创建代理, 尚未加载真实文档")
    proxy = VirtualProxy("/data/report.pdf")

    print("  获取元数据 (触发加载)...")
    meta = proxy.get_metadata()
    print(f"  元数据: 大小={meta['size_bytes']}bytes, 行数={meta['lines']}")

    print("  再次获取元数据 (已加载, 不再重新加载)...")
    meta2 = proxy.get_metadata()
    print(f"  元数据: checksum={meta2['checksum'][:8]}...")

    # ── 2. 保护代理: 权限控制 ──
    print("\n--- 2. 保护代理 (Protection Proxy): 权限控制 ---")
    real_doc = LargeDocument("/data/confidential.pdf")
    protected = ProtectedDocument(real_doc, owner=User("admin", "admin"))

    viewer = User("访客小王", "viewer")
    editor = User("编辑小李", "editor")
    hacker = User("骇客小黑", "hacker")

    for user in [viewer, editor, hacker]:
        print(f"\n  {user} 尝试查看文档:")
        result = protected.display(user)
        print(f"  -> {result}")

    # ── 3. 缓存代理: 减少重复计算 ──
    print("\n--- 3. 缓存代理 (Cache Proxy): 结果缓存 ---")
    real = LargeDocument("/data/analysis.pdf")
    cached_doc = CacheProxy(real, ttl_seconds=3.0)

    # 第一次访问 (会加载)
    print("\n  第一次调用 get_metadata()")
    m1 = cached_doc.get_metadata()

    # 第二次访问 (缓存命中)
    print("\n  第二次调用 get_metadata()")
    m2 = cached_doc.get_metadata()

    # 等待缓存过期
    print("\n  等待缓存过期...")
    time.sleep(3.5)

    # 第三次访问 (重新计算)
    print("\n  第三次调用 get_metadata() (缓存已过期)")
    m3 = cached_doc.get_metadata()

    # 手动失效
    print("\n  手动失效缓存后重新获取")
    cached_doc.invalidate("metadata")
    m4 = cached_doc.get_metadata()

    # ── 4. 远程代理: 隐藏网络细节 ──
    print("\n--- 4. 远程代理 (Remote Proxy): 封装远程调用 ---")
    weather = WeatherRemoteProxy()
    result = weather.get_weather("北京")
    print(f"  天气结果: {result}")
    forecast = weather.get_forecast("上海", 3)
    print(f"  预报: {forecast}")

    # ── 总结 ──
    print("\n" + "=" * 60)
    print("代理模式四种变体对比")
    print("=" * 60)
    print("  虚代理 (Virtual Proxy):  延迟加载, 节省资源")
    print("  保护代理 (Protection):   访问权限控制")
    print("  缓存代理 (Cache Proxy):  缓存结果, 减少计算")
    print("  远程代理 (Remote Proxy): 隐藏远程调用细节")
    print("=" * 60)


if __name__ == "__main__":
    main()
