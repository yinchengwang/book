#!/usr/bin/env python3
"""
工厂模式演示 —— 简单工厂 + 工厂方法 + 抽象工厂

本文件展示了三种工厂模式的递进关系:
1. 简单工厂 (Simple Factory): 集中式的对象创建逻辑
2. 工厂方法 (Factory Method): 通过子类工厂决定创建哪个产品
3. 抽象工厂 (Abstract Factory): 创建相关产品家族

应用场景: 跨平台 UI 组件系统和数据库连接创建。
"""

import abc
from typing import Dict, Type, Optional


# ============================================================
# 第一部分: 简单工厂 (Simple Factory)
# ============================================================
# 简单工厂不是 GoF 23 种模式之一, 但它是常用的编程惯用法。
# 用一个工厂类封装对象创建逻辑, 客户端只需要传入类型标识。

class PaymentMethod(abc.ABC):
    """抽象支付方式"""
    @abc.abstractmethod
    def pay(self, amount: float) -> str:
        pass


class Alipay(PaymentMethod):
    def pay(self, amount: float) -> str:
        return f"支付宝支付 ¥{amount:.2f} 成功"


class WechatPay(PaymentMethod):
    def pay(self, amount: float) -> str:
        return f"微信支付 ¥{amount:.2f} 成功"


class CreditCardPay(PaymentMethod):
    def pay(self, amount: float) -> str:
        return f"信用卡支付 ¥{amount:.2f} 成功 (含 0.5% 手续费)"


class PaymentFactory:
    """简单工厂 —— 根据类型创建支付对象"""

    _payments: Dict[str, Type[PaymentMethod]] = {
        "alipay": Alipay,
        "wechat": WechatPay,
        "credit": CreditCardPay,
    }

    @classmethod
    def create(cls, pay_type: str) -> PaymentMethod:
        """根据支付类型创建对应的支付对象"""
        cls_name = pay_type.lower()
        if cls_name not in cls._payments:
            raise ValueError(f"不支持的支付类型: {pay_type}, 可用: {list(cls._payments.keys())}")
        return cls._payments[cls_name]()

    @classmethod
    def register(cls, name: str, payment_cls: Type[PaymentMethod]) -> None:
        """动态注册新的支付方式 (开闭原则)"""
        cls._payments[name] = payment_cls


def test_simple_factory():
    print("[简单工厂] Simple Factory")
    factory = PaymentFactory()

    for pay_type in ["alipay", "wechat", "credit"]:
        payment = factory.create(pay_type)
        result = payment.pay(99.99)
        print(f"  {result}")
    print()


# ============================================================
# 第二部分: 工厂方法 (Factory Method)
# ============================================================
# 工厂方法定义一个创建对象的接口, 让子类决定实例化哪个类。
# 核心是: 延迟实例化到子类。

class Document(abc.ABC):
    """抽象文档"""
    @abc.abstractmethod
    def open(self) -> str:
        pass

    @abc.abstractmethod
    def save(self) -> str:
        pass

    @abc.abstractmethod
    def close(self) -> str:
        pass


class PDFDocument(Document):
    def open(self) -> str:
        return "PDF 文档已打开 (使用 PDF 阅读器)"

    def save(self) -> str:
        return "PDF 文档已保存"

    def close(self) -> str:
        return "PDF 文档已关闭"


class WordDocument(Document):
    def open(self) -> str:
        return "Word 文档已打开 (使用文字处理软件)"

    def save(self) -> str:
        return "Word 文档已保存 (格式: .docx)"

    def close(self) -> str:
        return "Word 文档已关闭"


class SpreadsheetDocument(Document):
    def open(self) -> str:
        return "电子表格已打开 (支持公式计算)"

    def save(self) -> str:
        return "电子表格已保存 (格式: .xlsx)"

    def close(self) -> str:
        return "电子表格已关闭"


class Application(abc.ABC):
    """抽象应用程序 —— 定义工厂方法 create_document()"""
    @abc.abstractmethod
    def create_document(self) -> Document:
        """工厂方法: 子类实现此方法来创建具体的 Document"""
        pass

    def new_document(self) -> Document:
        """客户端代码 —— 使用工厂方法"""
        doc = self.create_document()
        print(f"  {doc.open()}")
        print(f"  {doc.save()}")
        print(f"  {doc.close()}")
        return doc


class PDFApplication(Application):
    def create_document(self) -> Document:
        return PDFDocument()


class WordApplication(Application):
    def create_document(self) -> Document:
        return WordDocument()


class SpreadsheetApplication(Application):
    def create_document(self) -> Document:
        return SpreadsheetDocument()


def test_factory_method():
    print("[工厂方法] Factory Method")
    apps = {
        "PDF": PDFApplication(),
        "Word": WordApplication(),
        "Spreadsheet": SpreadsheetApplication(),
    }
    for name, app in apps.items():
        print(f"  [{name}]")
        app.new_document()
        print()


# ============================================================
# 第三部分: 抽象工厂 (Abstract Factory)
# ============================================================
# 抽象工厂创建相关或依赖的产品家族 (product family)。
# 核心是: 一组可互换的工厂, 生产一组可互换的产品。

class Button(abc.ABC):
    """抽象按钮组件"""
    @abc.abstractmethod
    def render(self) -> str:
        pass

    @abc.abstractmethod
    def click(self) -> str:
        pass


class Checkbox(abc.ABC):
    """抽象复选框组件"""
    @abc.abstractmethod
    def render(self) -> str:
        pass

    @abc.abstractmethod
    def check(self) -> str:
        pass


# --- Windows 风格 ---
class WindowsButton(Button):
    def render(self) -> str:
        return "  [Windows按钮] 渲染: 矩形按钮, 蓝色主题"

    def click(self) -> str:
        return "  [Windows按钮] 点击: 触发 windows.on_click 事件"


class WindowsCheckbox(Checkbox):
    def render(self) -> str:
        return "  [Windows复选框] 渲染: 带√标记的方框"

    def check(self) -> str:
        return "  [Windows复选框] 选中: 状态切换为 checked"


# --- macOS 风格 ---
class MacOSButton(Button):
    def render(self) -> str:
        return "  [macOS按钮] 渲染: 圆角按钮, 磨砂玻璃效果"

    def click(self) -> str:
        return "  [macOS按钮] 点击: 触发 cocoa.on_click 事件"


class MacOSCheckbox(Checkbox):
    def render(self) -> str:
        return "  [macOS复选框] 渲染: 圆形复选标记"

    def check(self) -> str:
        return "  [macOS复选框] 选中: 状态切换为 checked"


# --- Linux (GTK) 风格 ---
class LinuxButton(Button):
    def render(self) -> str:
        return "  [Linux按钮] 渲染: 棱角按钮, Adwaita 主题"

    def click(self) -> str:
        return "  [Linux按钮] 点击: 触发 gtk.on_click 事件"


class LinuxCheckbox(Checkbox):
    def render(self) -> str:
        return "  [Linux复选框] 渲染: 带×标记的方框"

    def check(self) -> str:
        return "  [Linux复选框] 选中: 状态切换为 checked"


class GUIFactory(abc.ABC):
    """抽象工厂 —— 创建 UI 产品家族"""
    @abc.abstractmethod
    def create_button(self) -> Button:
        pass

    @abc.abstractmethod
    def create_checkbox(self) -> Checkbox:
        pass


class WindowsFactory(GUIFactory):
    def create_button(self) -> Button:
        return WindowsButton()

    def create_checkbox(self) -> Checkbox:
        return WindowsCheckbox()


class MacOSFactory(GUIFactory):
    def create_button(self) -> Button:
        return MacOSButton()

    def create_checkbox(self) -> Checkbox:
        return MacOSCheckbox()


class LinuxFactory(GUIFactory):
    def create_button(self) -> Button:
        return LinuxButton()

    def create_checkbox(self) -> Checkbox:
        return LinuxCheckbox()


class ApplicationUI:
    """应用 UI —— 使用抽象工厂创建跨平台组件"""

    def __init__(self, factory: GUIFactory):
        self._factory = factory
        self._button = factory.create_button()
        self._checkbox = factory.create_checkbox()

    def render(self) -> None:
        print(f"  {self._button.render()}")
        print(f"  {self._checkbox.render()}")

    def interact(self) -> None:
        print(f"  {self._button.click()}")
        print(f"  {self._checkbox.check()}")


def test_abstract_factory():
    print("[抽象工厂] Abstract Factory")
    factories = {
        "Windows": WindowsFactory(),
        "macOS": MacOSFactory(),
        "Linux": LinuxFactory(),
    }
    for platform, factory in factories.items():
        print(f"  [{platform} UI]")
        app = ApplicationUI(factory)
        app.render()
        app.interact()
        print()


# ============================================================
# 第四部分: 工厂模式对比 Demo
# ============================================================

class DatabaseConnector(abc.ABC):
    """抽象数据库连接器"""
    @abc.abstractmethod
    def connect(self) -> str:
        pass

    @abc.abstractmethod
    def execute(self, sql: str) -> str:
        pass


class PostgreSQLConnector(DatabaseConnector):
    def connect(self) -> str:
        return "  PostgreSQL: 已连接到 localhost:5432"

    def execute(self, sql: str) -> str:
        return f"  PostgreSQL: 执行 {sql}, 返回 3 行"


class MySQLConnector(DatabaseConnector):
    def connect(self) -> str:
        return "  MySQL: 已连接到 localhost:3306"

    def execute(self, sql: str) -> str:
        return f"  MySQL: 执行 {sql}, 返回 5 行"


class SQLiteConnector(DatabaseConnector):
    def connect(self) -> str:
        return "  SQLite: 已打开本地数据库文件 test.db"

    def execute(self, sql: str) -> str:
        return f"  SQLite: 执行 {sql}, 返回 2 行"


class DatabaseFactory:
    """简单工厂 + 注册机制 —— 可扩展的数据库连接工厂"""

    _connectors: Dict[str, Type[DatabaseConnector]] = {}

    @classmethod
    def register(cls, name: str, connector_cls: Type[DatabaseConnector]) -> None:
        cls._connectors[name] = connector_cls

    @classmethod
    def create(cls, db_type: str) -> DatabaseConnector:
        if db_type not in cls._connectors:
            raise ValueError(f"不支持的数据库类型: {db_type}")
        return cls._connectors[db_type]()

    @classmethod
    def available_types(cls) -> list:
        return list(cls._connectors.keys())


# 注册数据库连接器
DatabaseFactory.register("postgresql", PostgreSQLConnector)
DatabaseFactory.register("mysql", MySQLConnector)
DatabaseFactory.register("sqlite", SQLiteConnector)


def test_database_factory():
    print("[数据库工厂] 简单工厂注册机制")
    for db_type in DatabaseFactory.available_types():
        conn = DatabaseFactory.create(db_type)
        print(f"  {conn.connect()}")
        print(f"  {conn.execute('SELECT * FROM users')}")
    print()


# ============================================================
# 主函数
# ============================================================

def main():
    print("=" * 60)
    print("工厂模式演示")
    print("=" * 60)
    print()

    test_simple_factory()
    test_factory_method()
    test_abstract_factory()
    test_database_factory()

    print("=" * 60)
    print("演示完成!")
    print("=" * 60)


if __name__ == "__main__":
    main()
