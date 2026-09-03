#!/usr/bin/env python3
"""
观察者模式 (Observer) 演示

定义对象间的一对多依赖关系, 当一个对象状态发生变化时,
所有依赖它的对象都会得到通知并自动更新。

本文件实现:
1. Subject/Observer 经典实现 (类方式)
2. EventEmitter 发布-订阅变体 (on/off/emit)
"""

from abc import ABC, abstractmethod
from typing import Any, Callable, Dict, List


# ============================================================
# 经典观察者模式: Subject + Observer
# ============================================================

class Observer(ABC):
    """抽象观察者接口"""
    @abstractmethod
    def update(self, event_type: str, data: Any) -> None:
        ...


class Subject:
    """主题 (可观察者) —— 维护观察者列表并通知变化"""

    def __init__(self, name: str = ""):
        self._name = name
        self._observers: List[Observer] = []

    def attach(self, observer: Observer) -> None:
        """注册观察者"""
        if observer not in self._observers:
            self._observers.append(observer)
            print(f"  [主题:{self._name}] 观察者 {type(observer).__name__} 已注册")

    def detach(self, observer: Observer) -> None:
        """注销观察者"""
        if observer in self._observers:
            self._observers.remove(observer)
            print(f"  [主题:{self._name}] 观察者 {type(observer).__name__} 已注销")

    def notify(self, event_type: str, data: Any) -> None:
        """广播通知所有观察者"""
        print(f"  [主题:{self._name}] 广播事件: {event_type}")
        for obs in self._observers:
            obs.update(event_type, data)


# ============================================================
# 具体观察者
# ============================================================

class EmailNotifier(Observer):
    def update(self, event_type: str, data: Any) -> None:
        print(f"    [邮件] 发送邮件通知: [{event_type}] {data}")

    def __repr__(self) -> str:
        return "EmailNotifier"


class SMSNotifier(Observer):
    def update(self, event_type: str, data: Any) -> None:
        print(f"    [短信] 发送短信通知: [{event_type}] {data}")

    def __repr__(self) -> str:
        return "SMSNotifier"


class PushNotifier(Observer):
    def update(self, event_type: str, data: Any) -> None:
        print(f"    [推送] APP 推送通知: [{event_type}] {data}")

    def __repr__(self) -> str:
        return "PushNotifier"


# ============================================================
# 订单系统 —— 使用经典观察者模式
# ============================================================

class OrderService(Subject):
    """订单服务 —— 订单状态变化时通知所有观察者"""

    def __init__(self):
        super().__init__("订单系统")
        self._orders: Dict[str, str] = {}

    def create_order(self, order_id: str) -> None:
        self._orders[order_id] = "已创建"
        self.notify("order.created", {"order_id": order_id, "status": "已创建"})

    def pay_order(self, order_id: str) -> None:
        self._orders[order_id] = "已支付"
        self.notify("order.paid", {"order_id": order_id, "status": "已支付"})

    def ship_order(self, order_id: str) -> None:
        self._orders[order_id] = "已发货"
        self.notify("order.shipped", {"order_id": order_id, "status": "已发货"})


# ============================================================
# 发布-订阅变体: EventEmitter (on/off/emit)
# ============================================================

class EventEmitter:
    """基于事件的发布-订阅模式实现, 比经典 Observer 更灵活"""

    def __init__(self):
        self._handlers: Dict[str, List[Callable]] = {}

    def on(self, event: str, handler: Callable) -> None:
        """订阅事件"""
        if event not in self._handlers:
            self._handlers[event] = []
        self._handlers[event].append(handler)
        print(f"  [EventEmitter] 订阅事件: {event}")

    def off(self, event: str, handler: Callable) -> None:
        """取消订阅事件"""
        handlers = self._handlers.get(event, [])
        if handler in handlers:
            handlers.remove(handler)
            print(f"  [EventEmitter] 取消订阅: {event}")

    def emit(self, event: str, *args, **kwargs) -> None:
        """触发事件, 通知所有订阅者"""
        print(f"  [EventEmitter] 触发事件: {event}")
        for handler in self._handlers.get(event, []):
            handler(*args, **kwargs)


# ============================================================
# 主函数
# ============================================================

def demo_order_system():
    """场景一: 订单系统的观察者模式"""
    print("\n" + "=" * 60)
    print("场景一: 订单系统通知 (经典 Observer)")
    print("=" * 60)

    service = OrderService()
    email = EmailNotifier()
    sms = SMSNotifier()
    push = PushNotifier()

    service.attach(email)
    service.attach(sms)
    service.attach(push)

    print()
    service.create_order("ORD-001")
    print()
    service.pay_order("ORD-001")
    print()
    service.ship_order("ORD-001")

    print("\n  [动态] 取消短信通知...")
    service.detach(sms)
    print()
    service.create_order("ORD-002")


def demo_event_emitter():
    """场景二: EventEmitter 发布-订阅模式"""
    print("\n" + "=" * 60)
    print("场景二: EventEmitter 发布-订阅模式")
    print("=" * 60)

    emitter = EventEmitter()

    # 订阅事件
    emitter.on("user.login", lambda uid: print(f"    [日志] 用户登录: {uid}"))
    emitter.on("user.login", lambda uid: print(f"    [审计] 记录登录事件, uid={uid}"))
    emitter.on("user.logout", lambda uid: print(f"    [日志] 用户登出: {uid}"))

    print()
    emitter.emit("user.login", uid=1001)
    print()
    emitter.emit("user.logout", uid=1001)


def main():
    print("=" * 60)
    print("观察者模式 (Observer) 演示")
    print("=" * 60)

    demo_order_system()
    demo_event_emitter()

    print("\n" + "=" * 60)
    print("演示完成!")
    print("=" * 60)


if __name__ == "__main__":
    main()
