"""
策略模式 (Strategy Pattern) - 支付系统 + 排序算法示例
定义一组算法，使其可以互换，让算法独立于使用它的客户端。
"""
from abc import ABC, abstractmethod
import time
import random


# ============ 策略接口 ============

class PaymentStrategy(ABC):
    """支付策略抽象基类"""

    @property
    @abstractmethod
    def fee_rate(self) -> float:
        """费率（百分比）"""
        pass

    @abstractmethod
    def pay(self, amount: float) -> str:
        """执行支付，返回交易凭证"""
        pass


# ============ 具体策略 ============

class CreditCardPayment(PaymentStrategy):
    @property
    def fee_rate(self) -> float:
        return 0.03  # 3%

    def pay(self, amount: float) -> str:
        fee = amount * self.fee_rate
        return f"[信用卡] 支付 CNY{amount:.2f}，手续费 CNY{fee:.2f}，实收 CNY{amount - fee:.2f}"


class AlipayPayment(PaymentStrategy):
    @property
    def fee_rate(self) -> float:
        return 0.005  # 0.5%

    def pay(self, amount: float) -> str:
        fee = amount * self.fee_rate
        return f"[支付宝] 支付 CNY{amount:.2f}，手续费 CNY{fee:.2f}，实收 CNY{amount - fee:.2f}"


class WechatPayment(PaymentStrategy):
    @property
    def fee_rate(self) -> float:
        return 0.006  # 0.6%

    def pay(self, amount: float) -> str:
        fee = amount * self.fee_rate
        return f"[微信支付] 支付 CNY{amount:.2f}，手续费 CNY{fee:.2f}，实收 CNY{amount - fee:.2f}"


class CryptoPayment(PaymentStrategy):
    @property
    def fee_rate(self) -> float:
        return 0.01  # 1%

    def pay(self, amount: float) -> str:
        fee = amount * self.fee_rate
        return f"[加密货币] 支付 CNY{amount:.2f}，GAS 费 CNY{fee:.2f}，实收 CNY{amount - fee:.2f}"


# ============ 上下文 ============

class PaymentContext:
    """支付上下文 —— 持有策略引用"""

    def __init__(self, strategy: PaymentStrategy | None = None) -> None:
        self._strategy = strategy

    @property
    def strategy(self) -> PaymentStrategy | None:
        return self._strategy

    @strategy.setter
    def strategy(self, strategy: PaymentStrategy) -> None:
        self._strategy = strategy

    def checkout(self, amount: float) -> None:
        if self._strategy is None:
            print("错误：未设置支付策略")
            return
        print(self._strategy.pay(amount))


# ============ 排序策略演示 ============

class SortStrategy(ABC):
    @abstractmethod
    def sort(self, data: list[int]) -> list[int]:
        pass


class QuickSort(SortStrategy):
    def sort(self, data: list[int]) -> list[int]:
        if len(data) <= 1:
            return data[:]
        pivot = data[len(data) // 2]
        left = [x for x in data if x < pivot]
        middle = [x for x in data if x == pivot]
        right = [x for x in data if x > pivot]
        return self.sort(left) + middle + self.sort(right)


class MergeSort(SortStrategy):
    def sort(self, data: list[int]) -> list[int]:
        if len(data) <= 1:
            return data[:]
        mid = len(data) // 2
        left = self.sort(data[:mid])
        right = self.sort(data[mid:])
        return self._merge(left, right)

    @staticmethod
    def _merge(left: list[int], right: list[int]) -> list[int]:
        result = []
        i = j = 0
        while i < len(left) and j < len(right):
            if left[i] <= right[j]:
                result.append(left[i])
                i += 1
            else:
                result.append(right[j])
                j += 1
        result.extend(left[i:])
        result.extend(right[j:])
        return result


# ============ if-else 对比 ============

def pay_with_if_else(method: str, amount: float) -> str:
    """if-else 方式：每新增一个支付方式都要修改此函数"""
    if method == "credit_card":
        fee = amount * 0.03
        return f"[信用卡] 支付 CNY{amount:.2f}，手续费 CNY{fee:.2f}"
    elif method == "alipay":
        fee = amount * 0.005
        return f"[支付宝] 支付 CNY{amount:.2f}，手续费 CNY{fee:.2f}"
    elif method == "wechat":
        fee = amount * 0.006
        return f"[微信] 支付 CNY{amount:.2f}，手续费 CNY{fee:.2f}"
    elif method == "crypto":
        fee = amount * 0.01
        return f"[加密货币] 支付 CNY{amount:.2f}，GAS 费 CNY{fee:.2f}"
    else:
        return f"错误：不支持的支付方式 '{method}'"


# ============ 主程序 ============

def main() -> None:
    print("=== 策略模式 — 支付系统演示 ===\n")

    context = PaymentContext()

    # 运行时切换策略
    for strategy in [
        CreditCardPayment(),
        AlipayPayment(),
        WechatPayment(),
        CryptoPayment(),
    ]:
        context.strategy = strategy
        amount = random.randint(50, 500)
        context.checkout(float(amount))

    print("\n=== 排序策略演示 ===")
    data = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5]
    print(f"原始数据: {data}")

    for strategy in [QuickSort(), MergeSort()]:
        t0 = time.perf_counter()
        result = strategy.sort(data)
        elapsed = (time.perf_counter() - t0) * 1000
        print(f"{type(strategy).__name__}: {result} ({elapsed:.2f}ms)")

    print("\n=== if-else 对比 ===")
    # 策略模式无需修改客户端代码即可扩展；if-else 需要修改 pay_with_if_else 函数
    print(pay_with_if_else("credit_card", 299.0))
    print(pay_with_if_else("alipay", 299.0))


if __name__ == "__main__":
    main()
