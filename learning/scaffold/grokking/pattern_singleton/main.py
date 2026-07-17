#!/usr/bin/env python3
"""
单例模式 (Singleton) 演示

确保一个类只有一个实例, 并提供全局访问点。

本文件实现 4 种单例风格:
1. 元类实现 (线程安全)
2. 装饰器实现
3. 模块级单例 (Python 最自然的方式)
4. __new__ 方法实现

以及单例在数据库连接池中的实际应用场景。
"""

import threading
import time
import random
from typing import Dict, Optional


# ============================================================
# 方式一: 元类实现 (线程安全)
# ============================================================

class SingletonMeta(type):
    """线程安全的单例元类 —— 双重检查锁定风格"""
    _instances: Dict[type, object] = {}
    _lock = threading.Lock()

    def __call__(cls, *args, **kwargs):
        # 先检查 (无需锁)
        if cls not in cls._instances:
            with cls._lock:
                # 再检查 (双重检查锁定 DCL)
                if cls not in cls._instances:
                    instance = super().__call__(*args, **kwargs)
                    cls._instances[cls] = instance
        return cls._instances[cls]


class ConfigManager(metaclass=SingletonMeta):
    """全局配置管理器 —— 使用元类实现单例"""

    def __init__(self):
        self._config: Dict[str, str] = {
            "db.host": "localhost",
            "db.port": "5432",
            "db.user": "admin",
            "app.name": "MyApp",
            "app.version": "1.0.0",
        }

    def get(self, key: str, default: Optional[str] = None) -> Optional[str]:
        return self._config.get(key, default)

    def set(self, key: str, value: str) -> None:
        self._config[key] = value

    def __repr__(self) -> str:
        items = ", ".join(f"{k}={v}" for k, v in self._config.items())
        return f"ConfigManager({items})"


# ============================================================
# 方式二: 装饰器实现
# ============================================================

def singleton(cls):
    """类装饰器实现单例模式"""
    _instances: Dict[type, object] = {}
    _lock = threading.Lock()

    def get_instance(*args, **kwargs):
        with _lock:
            if cls not in _instances:
                _instances[cls] = cls(*args, **kwargs)
        return _instances[cls]

    return get_instance


@singleton
class DatabaseConnectionPool:
    """数据库连接池 —— 使用装饰器实现单例"""

    def __init__(self, max_connections: int = 10):
        self._max_connections = max_connections
        self._active_connections: int = 0
        self._pool: list = []
        self._id = id(self)
        print(f"  [连接池] 初始化 (max={max_connections}, id={self._id})")

    def acquire(self) -> int:
        """获取一个连接"""
        if self._active_connections < self._max_connections:
            conn_id = self._active_connections
            self._active_connections += 1
            self._pool.append(conn_id)
            print(f"  [连接池] 获取连接 #{conn_id} (活跃: {self._active_connections})")
            return conn_id
        else:
            raise RuntimeError("连接池已满, 无法获取新连接")

    def release(self, conn_id: int) -> None:
        """释放一个连接"""
        if conn_id in self._pool:
            self._pool.remove(conn_id)
            self._active_connections -= 1
            print(f"  [连接池] 释放连接 #{conn_id} (活跃: {self._active_connections})")

    @property
    def active_count(self) -> int:
        return self._active_connections


# ============================================================
# 方式三: 模块级单例 (Python 推荐方式)
# ============================================================

class _Logger:
    """内部日志类 —— 通过模块变量暴露单例"""

    def __init__(self):
        self._level = "INFO"
        self._logs: list = []

    def info(self, message: str) -> None:
        self._logs.append(f"[INFO] {message}")
        print(f"  [INFO] {message}")

    def error(self, message: str) -> None:
        self._logs.append(f"[ERROR] {message}")
        print(f"  [ERROR] {message}")

    def warn(self, message: str) -> None:
        self._logs.append(f"[WARN] {message}")
        print(f"  [WARN] {message}")

    def get_logs(self) -> list:
        return self._logs


# 模块级单例 —— Python 模块天然是单例
logger = _Logger()


# ============================================================
# 方式四: __new__ 方法实现
# ============================================================

class ThreadSafeSingleton:
    """使用 __new__ 和锁实现线程安全单例"""
    _instance: Optional['ThreadSafeSingleton'] = None
    _lock = threading.Lock()
    _initialized: bool = False

    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:
                    cls._instance = super().__new__(cls)
        return cls._instance

    def __init__(self):
        # 防止重复初始化
        if not ThreadSafeSingleton._initialized:
            self._counter = 0
            ThreadSafeSingleton._initialized = True

    def increment(self) -> int:
        self._counter += 1
        return self._counter

    @property
    def counter(self) -> int:
        return self._counter


# ============================================================
# 多线程安全测试
# ============================================================

def test_config_singleton():
    print("[测试] 元类单例 ConfigManager")
    c1 = ConfigManager()
    c2 = ConfigManager()
    print(f"  c1 is c2: {c1 is c2}")

    c1.set("db.host", "192.168.1.1")
    assert c2.get("db.host") == "192.168.1.1"
    print(f"  c1.db.host={c1.get('db.host')}, c2.db.host={c2.get('db.host')}")
    print()


def test_pool_singleton():
    print("[测试] 装饰器单例 DatabaseConnectionPool")
    pool1 = DatabaseConnectionPool(5)
    pool2 = DatabaseConnectionPool(10)  # 参数被忽略, 返回同一个实例

    print(f"  pool1 is pool2: {pool1 is pool2}")
    print(f"  pool1.max={pool1._max_connections}, pool2.max={pool2._max_connections}")
    print()


def test_logger_singleton():
    print("[测试] 模块级单例 _Logger")
    logger.info("系统启动")
    logger.warn("磁盘空间不足")
    logger.error("连接超时")

    logs = logger.get_logs()
    print(f"  日志条数: {len(logs)}")
    for log in logs:
        print(f"    {log}")
    print()


def test_new_singleton():
    print("[测试] __new__ 单例 ThreadSafeSingleton")
    s1 = ThreadSafeSingleton()
    s2 = ThreadSafeSingleton()
    print(f"  s1 is s2: {s1 is s2}")
    print(f"  s1.counter={s1.increment()}, s2.counter={s2.increment()}")
    print()


def test_thread_safety():
    print("[测试] 多线程并发单例安全性")

    instances = []
    errors = []

    def create_config(idx: int):
        try:
            c = ConfigManager()
            instances.append(id(c))
        except Exception as e:
            errors.append(e)

    threads = []
    for i in range(20):
        t = threading.Thread(target=create_config, args=(i,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    unique_ids = set(instances)
    print(f"  线程数: {len(threads)}, 实例数: {len(instances)}, 唯一实例ID数: {len(unique_ids)}")
    print(f"  全部是同一实例: {len(unique_ids) == 1}")
    if errors:
        print(f"  错误: {errors}")
    print()


# ============================================================
# 应用场景: 数据库连接池管理
# ============================================================

def simulate_database_load():
    print("[场景] 模拟数据库负载 (多线程连接池使用)")
    pool = DatabaseConnectionPool(5)

    results = []

    def worker(worker_id: int):
        try:
            conn = pool.acquire()
            time.sleep(random.uniform(0.05, 0.15))
            pool.release(conn)
            results.append(f"  工作线程 #{worker_id} 完成")
        except RuntimeError as e:
            results.append(f"  工作线程 #{worker_id} 失败: {e}")

    workers = []
    for i in range(8):
        t = threading.Thread(target=worker, args=(i,))
        workers.append(t)
        t.start()

    for t in workers:
        t.join()

    for r in results:
        print(r)
    print()


# ============================================================
# 主函数
# ============================================================

def main():
    print("=" * 60)
    print("单例模式 (Singleton) 演示")
    print("=" * 60)
    print()

    # 四种实现方式
    test_config_singleton()
    test_pool_singleton()
    test_logger_singleton()
    test_new_singleton()

    # 多线程安全性
    test_thread_safety()

    # 实际应用场景
    simulate_database_load()

    print("=" * 60)
    print("演示完成!")
    print("=" * 60)


if __name__ == "__main__":
    main()
