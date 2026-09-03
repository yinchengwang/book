"""
反模式 (Anti-Patterns) 示例

展示三种常见反模式及其重构方案:
  1. GodClass  — 一个类承担过多职责
  2. HardCoded — 魔法数字/字符串散落代码中
  3. Spaghetti — 深层嵌套 + 全局状态
"""

import math
import random

# ====================================================================
# 反模式 1: GodClass（上帝类）
# ====================================================================

class GodClass:
    """反模式: 一个类负责用户管理、订单处理和库存更新三项职责."""

    def __init__(self):
        self.users: dict[str, dict] = {}
        self.orders: list[dict] = []
        self.inventory: dict[str, int] = {}

    def add_user(self, name: str, email: str) -> None:
        self.users[name] = {"email": email, "points": 0}
        print(f"  [God] 添加用户: {name}")

    def get_user_points(self, name: str) -> int:
        return self.users.get(name, {}).get("points", 0)

    def create_order(self, user: str, item: str, qty: int) -> int:
        oid = len(self.orders) + 1
        self.orders.append({"id": oid, "user": user, "item": item, "qty": qty})
        print(f"  [God] 创建订单 #{oid}: {user} x{item} x{qty}")
        # 上帝类还顺带扣库存...
        self._deduct_inventory(item, qty)
        return oid

    def _deduct_inventory(self, item: str, qty: int) -> None:
        if item not in self.inventory:
            self.inventory[item] = 100
        self.inventory[item] -= qty
        print(f"  [God] 扣减库存: {item} 剩余 {self.inventory[item]}")


# ── 重构: 职责分离为三个类 ─────────────────────────────────────

class UserManager:
    """用户管理: 只负责用户相关操作."""

    def __init__(self) -> None:
        self._users: dict[str, dict] = {}

    def add_user(self, name: str, email: str) -> None:
        self._users[name] = {"email": email, "points": 0}
        print(f"  [UserManager] 添加用户: {name}")

    def get_points(self, name: str) -> int:
        return self._users.get(name, {}).get("points", 0)


class InventoryManager:
    """库存管理: 只负责库存相关操作."""

    def __init__(self) -> None:
        self._stock: dict[str, int] = {}

    def deduct(self, item: str, qty: int) -> None:
        if item not in self._stock:
            self._stock[item] = 100
        self._stock[item] -= qty
        print(f"  [InventoryManager] 扣减库存: {item} 剩余 {self._stock[item]}")


class OrderManager:
    """订单管理: 只负责订单相关操作, 协作通过组合完成."""

    def __init__(self, users: UserManager, inventory: InventoryManager) -> None:
        self._orders: list[dict] = []
        self._users = users
        self._inventory = inventory

    def create_order(self, user: str, item: str, qty: int) -> int:
        oid = len(self._orders) + 1
        self._orders.append({"id": oid, "user": user, "item": item, "qty": qty})
        print(f"  [OrderManager] 创建订单 #{oid}")
        self._inventory.deduct(item, qty)
        return oid


# ====================================================================
# 反模式 2: HardCoded（硬编码/魔法数字）
# ====================================================================

def calc_price_before(amount: float) -> dict:
    """反模式: 魔法数字 0.15 含义不明, 维护时极易出错."""
    tax = amount * 0.15          # 0.15 是什么? 税率? 佣金?
    commission = amount * 0.15   # 又是 0.15, 但如果两个分别变化就麻烦了
    total = amount + tax + commission
    return {"tax": tax, "commission": commission, "total": total}


# ── 重构: 命名常量替代魔法数字 ─────────────────────────────────

TAX_RATE = 0.10        # 税率 10%
COMMISSION_RATE = 0.05 # 佣金率 5%

def calc_price_refactored(amount: float) -> dict:
    """重构: 使用有意义的常量名, 每个数字都有明确语义."""
    tax = amount * TAX_RATE
    commission = amount * COMMISSION_RATE
    total = amount + tax + commission
    return {"tax": tax, "commission": commission, "total": total}


# ====================================================================
# 反模式 3: Spaghetti（意大利面条式代码）
# ====================================================================

_DATA: list[int] = []  # 全局可变状态


def spaghetti_process(values: list[int]) -> list[int]:
    """
    反模式: 深层嵌套循环 + 全局状态修改 + 多个副作用混在一起.

    这个函数同时做了过滤、变换、全局累加, 逻辑交错难以理解.
    """
    result: list[int] = []
    for i in range(len(values)):          # 外层循环
        for j in range(i, len(values)):   # 中层循环
            if values[j] > 0:             # 条件过滤
                val = values[j] * 2       # 变换
                if val > 10:
                    val = val + 1
                    for k in range(3):    # 内层魔法循环
                        _DATA.append(val + k)  # 修改全局状态
                    result.append(val)
    return result


# ── 重构: 纯函数 + 清晰 pipeline ───────────────────────────────

def _filter_positive(nums: list[int]) -> list[int]:
    """过滤正数."""
    return [x for x in nums if x > 0]


def _double(nums: list[int]) -> list[int]:
    """翻倍."""
    return [x * 2 for x in nums]


def _cap_at_20(nums: list[int]) -> list[int]:
    """超过 20 的数截断到 20."""
    return [min(n, 20) for n in nums]


def _deduplicate_pairs(nums: list[int]) -> list[int]:
    """只保留每对中较大的一个 (spaghetti 中 j from i 的效果近似)."""
    seen: set[int] = set()
    out: list[int] = []
    for n in nums:
        if n not in seen:
            seen.add(n)
            out.append(n)
    return out


def clean_process(values: list[int]) -> list[int]:
    """
    重构: 每个函数只做一件事, pipeline 清晰可读.

    没有全局状态, 没有深层嵌套, 每个步骤可独立测试.
    """
    return _deduplicate_pairs(
        _cap_at_20(
            _double(
                _filter_positive(values)
            )
        )
    )


# ====================================================================
# 主函数
# ====================================================================

def demo_god_class() -> None:
    print("-" * 50)
    print("反模式 1: GodClass (上帝类)")
    print("-" * 50)
    print("  [反模式] GodClass 同时管理用户、订单、库存:")
    god = GodClass()
    god.add_user("Alice", "alice@example.com")
    god.create_order("Alice", "widget", 2)

    print()
    print("  [重构] 职责分离为 UserManager + OrderManager + InventoryManager:")
    um = UserManager()
    im = InventoryManager()
    om = OrderManager(um, im)
    um.add_user("Bob", "bob@example.com")
    om.create_order("Bob", "gadget", 3)
    print()


def demo_hard_coded() -> None:
    print("-" * 50)
    print("反模式 2: HardCoded (硬编码/魔法数字)")
    print("-" * 50)
    res_before = calc_price_before(100.0)
    print(f"  [反模式] 魔法数字 0.15: tax={res_before['tax']}, "
          f"commission={res_before['commission']}, total={res_before['total']}")
    print("          问题: 0.15 到底是什么? 税率和佣金同时变化怎么办?")

    res_after = calc_price_refactored(100.0)
    print(f"  [重构] 命名常量 TAX_RATE=0.10, COMMISSION_RATE=0.05:")
    print(f"          tax={res_after['tax']}, commission={res_after['commission']}, "
          f"total={res_after['total']}")
    print()


def demo_spaghetti() -> None:
    print("-" * 50)
    print("反模式 3: Spaghetti (面条式代码)")
    print("-" * 50)
    sample = [3, -1, 0, 8, 15, -5, 2]

    result_before = spaghetti_process(sample)
    print(f"  [反模式] 嵌套循环 + 全局状态: 结果={result_before}, "
          f"全局_DATA={_DATA}")

    result_after = clean_process(sample)
    print(f"  [重构] 纯函数 pipeline:    结果={result_after}")
    print("          每个步骤: filter_positive -> double -> cap_at_20 -> dedup")
    print()


def main() -> None:
    print("=" * 50)
    print("反模式 (Anti-Patterns) 演示")
    print("=" * 50)
    print()

    demo_god_class()
    demo_hard_coded()
    demo_spaghetti()

    print("=" * 50)
    print("反模式要点总结")
    print("=" * 50)
    print("  * GodClass:   违反单一职责原则 (SRP), 重构方式: 拆分为多个类")
    print("  * HardCoded:  魔法数字/字符串降低可维护性, 重构方式: 命名常量")
    print("  * Spaghetti:  深层嵌套 + 全局状态难以理解和测试, 重构: 纯函数 pipeline")
    print("=" * 50)


if __name__ == "__main__":
    main()
