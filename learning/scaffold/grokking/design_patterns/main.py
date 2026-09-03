#!/usr/bin/env python3
"""
GoF 23 种设计模式全景演示

本文件演示了来自三大类别的 6 种经典设计模式:
- 创建型: 单例 (Singleton), 工厂方法 (Factory Method)
- 结构型: 适配器 (Adapter), 装饰器 (Decorator)
- 行为型: 策略 (Strategy), 观察者 (Observer)

运行方式:
    python3 main.py

每个模式都有独立的测试用例, 输出清晰的边界标记。
"""

import abc
import threading
from typing import List, Optional, Callable


# ============================================================
# 第一部分: 创建型模式
# ============================================================

# --- 1.1 单例模式 (Singleton) ---
# 使用元类实现线程安全的单例

class SingletonMeta(type):
    """线程安全的单例元类"""
    _instances = {}
    _lock = threading.Lock()

    def __call__(cls, *args, **kwargs):
        with cls._lock:
            if cls not in cls._instances:
                instance = super().__call__(*args, **kwargs)
                cls._instances[cls] = instance
        return cls._instances[cls]


class DatabaseConfig(metaclass=SingletonMeta):
    """数据库全局配置 —— 整个进程只有一个配置实例"""

    def __init__(self):
        self._host = "localhost"
        self._port = 5432
        self._dbname = "testdb"

    @property
    def host(self) -> str:
        return self._host

    @host.setter
    def host(self, value: str) -> None:
        self._host = value

    @property
    def port(self) -> int:
        return self._port

    @property
    def dbname(self) -> str:
        return self._dbname

    def __repr__(self) -> str:
        return f"DatabaseConfig(host={self._host}, port={self._port}, db={self._dbname})"


def test_singleton():
    print("[创建型] 单例模式 Singleton")
    c1 = DatabaseConfig()
    c2 = DatabaseConfig()
    assert c1 is c2, "c1 和 c2 必须是同一个实例"
    c1.host = "192.168.1.100"
    assert c2.host == "192.168.1.100", "修改 c1 应影响 c2"
    print(f"  c1: {c1}")
    print(f"  c2: {c2}")
    print(f"  同一实例: {c1 is c2}")
    print()


# --- 1.2 工厂方法模式 (Factory Method) ---
class Logger(abc.ABC):
    """抽象日志记录器"""

    @abc.abstractmethod
    def log(self, message: str) -> None:
        pass


class ConsoleLogger(Logger):
    def log(self, message: str) -> None:
        print(f"[Console] {message}")


class FileLogger(Logger):
    def __init__(self):
        self._logs: List[str] = []

    def log(self, message: str) -> None:
        self._logs.append(message)
        print(f"[File] {message} (已缓存 {len(self._logs)} 条)")

    def flush(self) -> None:
        print(f"[File] 已将 {len(self._logs)} 条日志写入磁盘: {self._logs}")
        self._logs.clear()


class LoggerFactory(abc.ABC):
    """抽象工厂 —— 定义创建日志记录器的接口"""
    @abc.abstractmethod
    def create_logger(self) -> Logger:
        pass


class ConsoleLoggerFactory(LoggerFactory):
    def create_logger(self) -> Logger:
        return ConsoleLogger()


class FileLoggerFactory(LoggerFactory):
    def create_logger(self) -> Logger:
        return FileLogger()


def test_factory_method():
    print("[创建型] 工厂方法 Factory Method")
    factories = {
        "console": ConsoleLoggerFactory(),
        "file": FileLoggerFactory(),
    }
    for name, factory in factories.items():
        logger = factory.create_logger()
        logger.log(f"使用 {name} 记录器输出消息")
    print()


# ============================================================
# 第二部分: 结构型模式
# ============================================================

# --- 2.1 适配器模式 (Adapter) ---
class USBPowerSupply:
    """美标电源 (120V)"""
    def output_120v(self) -> int:
        return 120


class ChinaDevice:
    """中国电器 (220V)"""
    def operate(self, voltage: int) -> str:
        if voltage >= 200:
            return f"正常运行在 {voltage}V"
        return f"电压 {voltage}V 不足, 无法运行"


class VoltageAdapter:
    """电源适配器: 将 120V 转换为 220V"""

    def __init__(self, usb_power: USBPowerSupply):
        self._usb_power = usb_power

    def output_220v(self) -> int:
        # 升压转换 (模拟)
        return self._usb_power.output_120v() * 2 - 20


def test_adapter():
    print("[结构型] 适配器模式 Adapter")
    us_power = USBPowerSupply()
    adapter = VoltageAdapter(us_power)
    device = ChinaDevice()

    # 直接使用美标电源 (失败)
    direct_voltage = us_power.output_120v()
    result_direct = device.operate(direct_voltage)
    print(f"  直接连接美标电源: {result_direct}")

    # 通过适配器 (成功)
    adapted_voltage = adapter.output_220v()
    result_adapted = device.operate(adapted_voltage)
    print(f"  通过适配器连接: {result_adapted} (电压={adapted_voltage}V)")
    print()


# --- 2.2 装饰器模式 (Decorator) ---
class Coffee(abc.ABC):
    """抽象咖啡组件"""
    @abc.abstractmethod
    def cost(self) -> float:
        pass

    @abc.abstractmethod
    def description(self) -> str:
        pass


class BlackCoffee(Coffee):
    """黑咖啡 —— 基础组件"""
    def cost(self) -> float:
        return 15.0

    def description(self) -> str:
        return "黑咖啡"


class CoffeeDecorator(Coffee, abc.ABC):
    """咖啡装饰器基类"""

    def __init__(self, coffee: Coffee):
        self._coffee = coffee

    @abc.abstractmethod
    def cost(self) -> float:
        pass

    @abc.abstractmethod
    def description(self) -> str:
        pass


class MilkDecorator(CoffeeDecorator):
    """加奶装饰器"""
    def cost(self) -> float:
        return self._coffee.cost() + 5.0

    def description(self) -> str:
        return self._coffee.description() + " + 牛奶"


class SugarDecorator(CoffeeDecorator):
    """加糖装饰器"""
    def cost(self) -> float:
        return self._coffee.cost() + 2.0

    def description(self) -> str:
        return self._coffee.description() + " + 糖"


class WhippedCreamDecorator(CoffeeDecorator):
    """加奶油装饰器"""
    def cost(self) -> float:
        return self._coffee.cost() + 8.0

    def description(self) -> str:
        return self._coffee.description() + " + 奶油"


def test_decorator():
    print("[结构型] 装饰器模式 Decorator")
    # 基础黑咖啡
    coffee = BlackCoffee()
    print(f"  基础: {coffee.description()} = ¥{coffee.cost():.1f}")

    # 加牛奶
    coffee = MilkDecorator(BlackCoffee())
    print(f"  加牛奶: {coffee.description()} = ¥{coffee.cost():.1f}")

    # 加牛奶+糖
    coffee = SugarDecorator(MilkDecorator(BlackCoffee()))
    print(f"  加牛奶+糖: {coffee.description()} = ¥{coffee.cost():.1f}")

    # 黑咖啡+奶油+糖
    coffee = SugarDecorator(WhippedCreamDecorator(BlackCoffee()))
    print(f"  奶油+糖: {coffee.description()} = ¥{coffee.cost():.1f}")
    print()


# ============================================================
# 第三部分: 行为型模式
# ============================================================

# --- 3.1 策略模式 (Strategy) ---
class SortStrategy(abc.ABC):
    """排序策略接口"""
    @abc.abstractmethod
    def sort(self, data: List[int]) -> List[int]:
        pass

    @property
    @abc.abstractmethod
    def name(self) -> str:
        pass


class BubbleSortStrategy(SortStrategy):
    """冒泡排序策略"""
    @property
    def name(self) -> str:
        return "冒泡排序"

    def sort(self, data: List[int]) -> List[int]:
        result = list(data)
        n = len(result)
        for i in range(n):
            for j in range(0, n - i - 1):
                if result[j] > result[j + 1]:
                    result[j], result[j + 1] = result[j + 1], result[j]
        return result


class QuickSortStrategy(SortStrategy):
    """快速排序策略"""
    @property
    def name(self) -> str:
        return "快速排序"

    def sort(self, data: List[int]) -> List[int]:
        result = list(data)
        self._quicksort(result, 0, len(result) - 1)
        return result

    def _quicksort(self, arr: List[int], low: int, high: int) -> None:
        if low < high:
            pi = self._partition(arr, low, high)
            self._quicksort(arr, low, pi - 1)
            self._quicksort(arr, pi + 1, high)

    def _partition(self, arr: List[int], low: int, high: int) -> int:
        pivot = arr[high]
        i = low - 1
        for j in range(low, high):
            if arr[j] <= pivot:
                i += 1
                arr[i], arr[j] = arr[j], arr[i]
        arr[i + 1], arr[high] = arr[high], arr[i + 1]
        return i + 1


class Sorter:
    """排序上下文 —— 使用策略模式切换算法"""

    def __init__(self, strategy: SortStrategy):
        self._strategy = strategy

    def set_strategy(self, strategy: SortStrategy) -> None:
        self._strategy = strategy

    def sort(self, data: List[int]) -> List[int]:
        print(f"  [{self._strategy.name}] 排序: {data}")
        result = self._strategy.sort(data)
        print(f"  结果: {result}")
        return result


def test_strategy():
    print("[行为型] 策略模式 Strategy")
    data = [64, 34, 25, 12, 22, 11, 90]

    sorter = Sorter(BubbleSortStrategy())
    sorter.sort(data)

    sorter.set_strategy(QuickSortStrategy())
    sorter.sort(data)
    print()


# --- 3.2 观察者模式 (Observer) ---
class Observer(abc.ABC):
    """观察者接口"""
    @abc.abstractmethod
    def update(self, temperature: float, humidity: float, pressure: float) -> None:
        pass


class Subject(abc.ABC):
    """主题接口"""
    @abc.abstractmethod
    def register_observer(self, observer: Observer) -> None:
        pass

    @abc.abstractmethod
    def remove_observer(self, observer: Observer) -> None:
        pass

    @abc.abstractmethod
    def notify_observers(self) -> None:
        pass


class WeatherData(Subject):
    """气象数据 —— 被观察的主题"""

    def __init__(self):
        self._observers: List[Observer] = []
        self._temperature = 0.0
        self._humidity = 0.0
        self._pressure = 0.0

    def register_observer(self, observer: Observer) -> None:
        self._observers.append(observer)

    def remove_observer(self, observer: Observer) -> None:
        self._observers.remove(observer)

    def notify_observers(self) -> None:
        for obs in self._observers:
            obs.update(self._temperature, self._humidity, self._pressure)

    def set_measurements(self, temp: float, humidity: float, pressure: float) -> None:
        self._temperature = temp
        self._humidity = humidity
        self._pressure = pressure
        self.notify_observers()


class CurrentConditionsDisplay(Observer):
    """当前天气显示板"""
    def update(self, temperature: float, humidity: float, pressure: float) -> None:
        print(f"  [当前天气] 温度={temperature:.1f}°C, 湿度={humidity:.1f}%, 气压={pressure:.1f}hPa")


class StatisticsDisplay(Observer):
    """统计显示板 (追踪极值)"""
    def __init__(self):
        self._max_temp = float('-inf')
        self._min_temp = float('inf')
        self._total_temp = 0.0
        self._count = 0

    def update(self, temperature: float, humidity: float, pressure: float) -> None:
        self._max_temp = max(self._max_temp, temperature)
        self._min_temp = min(self._min_temp, temperature)
        self._total_temp += temperature
        self._count += 1
        avg = self._total_temp / self._count
        print(f"  [统计摘要] 最高={self._max_temp:.1f}°C, 最低={self._min_temp:.1f}°C, 平均={avg:.1f}°C")


class ForecastDisplay(Observer):
    """天气预报显示板"""
    def update(self, temperature: float, humidity: float, pressure: float) -> None:
        if pressure > 1020:
            forecast = "天气晴朗"
        elif pressure > 1005:
            forecast = "多云"
        else:
            forecast = "可能有雨"
        print(f"  [天气预报] {forecast} (气压={pressure:.1f}hPa)")


def test_observer():
    print("[行为型] 观察者模式 Observer")
    weather = WeatherData()

    current = CurrentConditionsDisplay()
    stats = StatisticsDisplay()
    forecast = ForecastDisplay()

    weather.register_observer(current)
    weather.register_observer(stats)
    weather.register_observer(forecast)

    print("  第一次更新:")
    weather.set_measurements(25.0, 65.0, 1013.0)
    print("  第二次更新:")
    weather.set_measurements(28.0, 70.0, 998.0)
    print("  第三次更新:")
    weather.set_measurements(22.0, 80.0, 1030.0)
    print()


# ============================================================
# 主函数
# ============================================================

def main():
    print("=" * 60)
    print("GoF 23 种设计模式全景演示")
    print("=" * 60)
    print()

    test_singleton()
    test_factory_method()
    test_adapter()
    test_decorator()
    test_strategy()
    test_observer()

    print("=" * 60)
    print("所有演示完成!")
    print("=" * 60)


if __name__ == "__main__":
    main()
