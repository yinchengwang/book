#!/usr/bin/env python3
"""
外观模式 (Facade Pattern) 示例

为复杂子系统提供统一的高层接口。
本示例模拟一个在线购物系统：
- 子系统包括：库存管理、支付处理、物流配送、通知服务
- Facade: 购物车结算服务，对外提供 checkot() 单一入口
"""

from abc import ABC, abstractmethod
from typing import Dict, List, Optional, Tuple


# ==================== 子系统：库存管理 ====================

class InventoryService:
    """库存子系统：管理商品库存"""

    def __init__(self) -> None:
        self._stock: Dict[str, int] = {
            "book_python": 10,
            "book_cpp": 5,
            "laptop": 3,
            "mouse": 20,
            "keyboard": 15,
        }
        self._reserved: Dict[str, int] = {}

    def check_availability(self, item_id: str, quantity: int) -> bool:
        """检查商品是否有足够库存"""
        available = self._stock.get(item_id, 0)
        return available >= quantity

    def reserve_items(self, item_id: str, quantity: int) -> bool:
        """预留商品（防止超卖）"""
        if not self.check_availability(item_id, quantity):
            return False
        self._stock[item_id] = self._stock.get(item_id, 0) - quantity
        self._reserved[item_id] = self._reserved.get(item_id, 0) + quantity
        return True

    def release_reservation(self, item_id: str, quantity: int) -> None:
        """释放预留（取消订单时）"""
        reserved = self._reserved.get(item_id, 0)
        if reserved >= quantity:
            self._reserved[item_id] = reserved - quantity
            self._stock[item_id] = self._stock.get(item_id, 0) + quantity

    def confirm_deduction(self, item_id: str, quantity: int) -> None:
        """确认扣减（订单完成时从预留中移除）"""
        reserved = self._reserved.get(item_id, 0)
        if reserved >= quantity:
            self._reserved[item_id] = reserved - quantity

    def get_stock_summary(self) -> Dict[str, int]:
        """获取库存快照"""
        return dict(self._stock)


# ==================== 子系统：支付处理 ====================

class PaymentResult:
    """支付结果"""

    def __init__(self, success: bool, transaction_id: str, message: str) -> None:
        self.success = success
        self.transaction_id = transaction_id
        self.message = message

    def __repr__(self) -> str:
        status = "成功" if self.success else "失败"
        return f"[{status}] {self.message} (交易号: {self.transaction_id})"


class PaymentService:
    """支付子系统：处理支付"""

    def __init__(self) -> None:
        self._accounts: Dict[str, float] = {
            "alice": 1000.0,
            "bob": 500.0,
            "shop": 0.0,
        }
        self._transactions: List[str] = []

    def charge(self, user_id: str, amount: float) -> PaymentResult:
        """从用户账户扣款"""
        import uuid
        txn_id = f"TXN-{uuid.uuid4().hex[:8].upper()}"

        balance = self._accounts.get(user_id, 0.0)
        if balance < amount:
            return PaymentResult(False, txn_id,
                                 f"余额不足: 需要 {amount:.2f}, 可用 {balance:.2f}")

        self._accounts[user_id] = balance - amount
        self._accounts["shop"] += amount
        self._transactions.append(txn_id)
        return PaymentResult(True, txn_id, f"扣款 {amount:.2f} 成功")

    def refund(self, transaction_id: str, user_id: str, amount: float) -> PaymentResult:
        """退款到用户账户"""
        self._accounts[user_id] = self._accounts.get(user_id, 0.0) + amount
        self._accounts["shop"] -= amount
        return PaymentResult(True, f"RFN-{transaction_id}",
                             f"退款 {amount:.2f} 成功")

    def get_balance(self, user_id: str) -> float:
        return self._accounts.get(user_id, 0.0)


# ==================== 子系统：物流配送 ====================

class ShippingResult:
    """物流结果"""

    def __init__(self, tracking_id: str, estimated_days: int) -> None:
        self.tracking_id = tracking_id
        self.estimated_days = estimated_days

    def __repr__(self) -> str:
        return (f"物流单号: {self.tracking_id}, "
                f"预计 {self.estimated_days} 天送达")


class ShippingService:
    """物流子系统：处理配送"""

    def __init__(self) -> None:
        self._shipments: Dict[str, str] = {}

    def create_shipment(self, address: str, items: List[str]) -> ShippingResult:
        """创建配送单"""
        import uuid
        tracking = f"SF-{uuid.uuid4().hex[:8].upper()}"
        self._shipments[tracking] = address
        # 假设配送时间基于地址复杂度估算
        estimated_days = 3 if "偏远" not in address else 7
        return ShippingResult(tracking, estimated_days)

    def track_shipment(self, tracking_id: str) -> Optional[str]:
        """查询配送状态"""
        address = self._shipments.get(tracking_id)
        if address:
            return f"配送中 -> {address}"
        return None


# ==================== 子系统：通知服务 ====================

class NotificationService:
    """通知子系统：发送各种通知"""

    @staticmethod
    def send_email(user_id: str, subject: str, body: str) -> str:
        """发送邮件通知"""
        return f"[邮件] 致 {user_id}: {subject} - {body[:50]}..."

    @staticmethod
    def send_sms(phone: str, message: str) -> str:
        """发送短信通知"""
        return f"[短信] 致 {phone}: {message[:40]}..."

    @staticmethod
    def send_in_app(user_id: str, message: str) -> str:
        """发送应用内通知"""
        return f"[应用内] 致 {user_id}: {message}"


# ==================== 订单项 ====================

class OrderItem:
    """订单中的商品项"""

    def __init__(self, item_id: str, name: str, quantity: int, price: float) -> None:
        self.item_id = item_id
        self.name = name
        self.quantity = quantity
        self.price = price

    @property
    def subtotal(self) -> float:
        return self.quantity * self.price

    def __repr__(self) -> str:
        return f"{self.name} x{self.quantity} = {self.subtotal:.2f}"


# ==================== 外观 (Facade) ====================

class CheckoutFacade:
    """
    结算外观：为购物车结算提供统一的高层接口

    封装了库存检查、支付、物流、通知等多个子系统
    """

    def __init__(self) -> None:
        self._inventory = InventoryService()
        self._payment = PaymentService()
        self._shipping = ShippingService()
        self._notifier = NotificationService()
        self._order_history: Dict[str, List[str]] = {}

    def checkout(self, user_id: str, items: List[OrderItem],
                 address: str, phone: str = "") -> Dict[str, str]:
        """
        统一结算入口

        参数:
            user_id: 用户标识
            items: 订单商品列表
            address: 收货地址
            phone: 联系电话 (用于短信通知)

        返回:
            包含各步骤结果的字典
        """
        result: Dict[str, str] = {}

        # Step 1: 检查库存
        result["步骤1-库存检查"] = self._check_inventory(items)

        # Step 2: 计算总价
        total = sum(item.subtotal for item in items)
        result["步骤2-计算总价"] = f"订单总额: {total:.2f}"

        # Step 3: 预留库存 (防止支付期间被抢)
        if not self._reserve_inventory(items):
            result["步骤3-预留库存"] = "预留失败"
            self._rollback(items)
            return result
        result["步骤3-预留库存"] = "预留成功"

        # Step 4: 处理支付
        pay_result = self._payment.charge(user_id, total)
        result["步骤4-支付处理"] = str(pay_result)
        if not pay_result.success:
            self._release_inventory(items)
            return result

        # Step 5: 确认扣减库存
        self._confirm_inventory(items)

        # Step 6: 创建物流
        item_names = [it.name for it in items]
        ship_result = self._shipping.create_shipment(address, item_names)
        result["步骤5-物流配送"] = str(ship_result)

        # Step 7: 发送通知
        email_log = self._notifier.send_email(
            user_id, "订单确认",
            f"您的订单已确认，物流单号: {ship_result.tracking_id}"
        )
        result["步骤6-通知"] = email_log
        if phone:
            sms_log = self._notifier.send_sms(phone, "订单已确认，祝您购物愉快")
            result["步骤6-短信通知"] = sms_log

        # 记录历史
        self._order_history.setdefault(user_id, []).append(
            f"订单 {pay_result.transaction_id}: {total:.2f} -> {ship_result.tracking_id}"
        )

        return result

    def get_order_history(self, user_id: str) -> List[str]:
        """查询订单历史"""
        return self._order_history.get(user_id, [])

    # ---------- 内部辅助方法 ----------

    def _check_inventory(self, items: List[OrderItem]) -> str:
        """检查所有商品的库存"""
        for item in items:
            if not self._inventory.check_availability(item.item_id, item.quantity):
                return f"库存不足: {item.name} (需要 {item.quantity}, 可用不足)"
        return "库存充足"

    def _reserve_inventory(self, items: List[OrderItem]) -> bool:
        """预留所有商品库存"""
        for item in items:
            if not self._inventory.reserve_items(item.item_id, item.quantity):
                return False
        return True

    def _confirm_inventory(self, items: List[OrderItem]) -> None:
        """确认扣减库存"""
        for item in items:
            self._inventory.confirm_deduction(item.item_id, item.quantity)

    def _release_inventory(self, items: List[OrderItem]) -> None:
        """释放预留库存"""
        for item in items:
            self._inventory.release_reservation(item.item_id, item.quantity)

    def _rollback(self, items: List[OrderItem]) -> None:
        """完整回滚（预留前无需操作）"""
        pass


# ==================== 主函数：演示外观模式 ====================

def main() -> None:
    print("=" * 60)
    print("外观模式 (Facade Pattern) 演示 - 在线购物结算")
    print("=" * 60)

    # 创建外观
    facade = CheckoutFacade()

    # 正常结算流程
    print("\n[场景 1] 正常结算")
    items = [
        OrderItem("book_python", "Python 编程书", 2, 79.0),
        OrderItem("mouse", "无线鼠标", 1, 49.0),
    ]
    result = facade.checkout("alice", items, "北京市海淀区中关村大街1号", "13800138000")
    for step, msg in result.items():
        print(f"  {step}: {msg}")
    print()

    # 查看订单历史
    print("[场景 1] 订单历史")
    for record in facade.get_order_history("alice"):
        print(f"  {record}")
    print()

    # 余额不足
    print("[场景 2] 余额不足")
    items2 = [
        OrderItem("laptop", "高性能笔记本电脑", 2, 9999.0),
    ]
    result2 = facade.checkout("bob", items2, "上海市浦东新区")
    for step, msg in result2.items():
        print(f"  {step}: {msg}")
    print()

    # 库存不足
    print("[场景 3] 库存不足")
    items3 = [
        OrderItem("book_cpp", "C++ 编程书", 100, 89.0),
    ]
    result3 = facade.checkout("alice", items3, "广州市天河区")
    for step, msg in result3.items():
        print(f"  {step}: {msg}")
    print()

    # 展示封装性
    print("[观察] 客户端只调用 facade.checkout()，不需要了解：")
    print("  - InventoryService (库存管理)")
    print("  - PaymentService (支付处理)")
    print("  - ShippingService (物流配送)")
    print("  - NotificationService (通知服务)")
    print("  所有子系统变化对客户透明")
    print()

    print("=" * 60)
    print("总结")
    print("  Facade 简化了复杂子系统的使用")
    print("  客户只需要了解一个接口 (checkout)")
    print("  子系统可以独立演化，不影响客户端")
    print("  Facade 不限制直接访问子系统 (不是防火墙)")
    print("=" * 60)


if __name__ == "__main__":
    main()
