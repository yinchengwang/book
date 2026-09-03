#!/usr/bin/env python3
"""
testing.py — Python 测试演示

演示 Python 常用测试方法：unittest + pytest + mock。
"""

import unittest
from unittest.mock import Mock, patch, MagicMock
from typing import List


# ============================================================================
# 1. unittest 基础
# ============================================================================

class TestMath(unittest.TestCase):
    """unittest 测试类"""

    def test_add(self):
        """测试加法"""
        self.assertEqual(1 + 1, 2)

    def test_divide(self):
        """测试除法"""
        self.assertEqual(10 / 2, 5)

    def test_divide_by_zero(self):
        """测试零除异常"""
        with self.assertRaises(ZeroDivisionError):
            1 / 0

    def test_list_operations(self):
        """测试列表操作"""
        lst = [1, 2, 3]
        lst.append(4)
        self.assertEqual(len(lst), 4)
        self.assertIn(4, lst)


# ============================================================================
# 2. pytest 风格测试
# ============================================================================

def test_pytest_style():
    """pytest 风格断言"""
    # 这些断言在 pytest 中自动收集
    assert 2 + 2 == 4
    assert "hello".upper() == "HELLO"
    assert [1, 2, 3] == [1, 2, 3]


def add(a: int, b: int) -> int:
    """待测试函数"""
    return a + b


def divide(a: float, b: float) -> float:
    """待测试函数"""
    if b == 0:
        raise ZeroDivisionError("Cannot divide by zero")
    return a / b


def find_max(numbers: List[int]) -> int:
    """查找最大值"""
    if not numbers:
        raise ValueError("Empty list")
    return max(numbers)


# ============================================================================
# 3. Mock 和 Patch
# ============================================================================

def get_weather(city: str) -> str:
    """模拟获取天气（实际调用外部 API）"""
    # 假设这里调用真实 API
    return f"Weather in {city}: Sunny, 25°C"


class WeatherService:
    """天气服务"""
    def get_weather(self, city: str) -> str:
        return get_weather(city)


def test_with_mock():
    """使用 Mock 测试"""
    with patch('__main__.get_weather') as mock_get:
        mock_get.return_value = "Mocked Weather"
        service = WeatherService()
        result = service.get_weather("Beijing")
        assert result == "Mocked Weather"
        mock_get.assert_called_once_with("Beijing")


# ============================================================================
# 4. MagicMock
# ============================================================================

def test_magic_mock():
    """MagicMock 自动创建属性和方法"""
    mock_obj = MagicMock()
    mock_obj.method.return_value = "result"
    mock_obj.attribute = "value"

    assert mock_obj.method() == "result"
    assert mock_obj.attribute == "value"


# ============================================================================
# 5. Spy
# ============================================================================

def test_spy():
    """Spy 记录调用但不替换实现"""
    original_list = [3, 1, 2]
    original_max = max

    spy_max = Mock(side_effect=lambda x: original_max(x) if isinstance(x, list) else original_max(x))
    result = spy_max(original_list)
    assert result == 3


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 测试演示")
    print("=" * 60)

    # 运行 unittest
    print("\n[1] unittest:")
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromTestCase(TestMath)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    # pytest 风格测试
    print("\n[2] pytest 风格:")
    test_pytest_style()
    print("    所有断言通过 [OK]")

    # Mock 测试
    print("\n[3] Mock 测试:")
    test_with_mock()
    print("    Mock 测试通过 [OK]")

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
