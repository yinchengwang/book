"""
组合模式 (Composite Pattern) 示例

将对象组合成树形结构以表示部分-整体层次, 使客户代码可以
一致地处理简单和复杂元素。

本示例展示: 文件系统、组织架构、表达式计算。
"""

import abc
from dataclasses import dataclass, field
from typing import Optional


# =================================================================
# 公共组件接口
# =================================================================

class FileSystemNode(abc.ABC):
    """文件系统节点接口 (Component)"""

    @abc.abstractmethod
    def get_name(self) -> str:
        """获取节点名称"""
        ...

    @abc.abstractmethod
    def get_size(self) -> int:
        """获取节点大小 (字节)"""
        ...

    @abc.abstractmethod
    def display(self, indent: str = "") -> str:
        """显示节点树形结构"""
        ...

    def search(self, name: str) -> list["FileSystemNode"]:
        """搜索节点 (默认实现: 不包含子节点)"""
        if self.get_name() == name:
            return [self]
        return []


# =================================================================
# 叶子节点: 文件
# =================================================================

class File(FileSystemNode):
    """叶子节点: 文件"""

    def __init__(self, name: str, size: int = 0):
        self._name = name
        self._size = size

    def get_name(self) -> str:
        return self._name

    def get_size(self) -> int:
        return self._size

    def display(self, indent: str = "") -> str:
        line = f"{indent}📄 {self._name} ({self._size} bytes)"
        return line


# =================================================================
# 组合节点: 目录
# =================================================================

class Directory(FileSystemNode):
    """组合节点: 目录, 可包含文件和子目录"""

    def __init__(self, name: str):
        self._name = name
        self._children: list[FileSystemNode] = []

    def get_name(self) -> str:
        return self._name

    def add(self, node: FileSystemNode) -> "Directory":
        """添加子节点"""
        self._children.append(node)
        return self  # 支持链式调用

    def remove(self, node: FileSystemNode) -> bool:
        """移除子节点"""
        if node in self._children:
            self._children.remove(node)
            return True
        return False

    def get_children(self) -> list[FileSystemNode]:
        """获取所有子节点"""
        return list(self._children)

    def get_size(self) -> int:
        """递归计算目录总大小"""
        total = 0
        for child in self._children:
            total += child.get_size()
        return total

    def display(self, indent: str = "") -> str:
        lines = [f"{indent}📁 {self._name}/ ({self.get_size()} bytes)"]
        for child in self._children:
            lines.append(child.display(indent + "  "))
        return "\n".join(lines)

    def search(self, name: str) -> list[FileSystemNode]:
        results = []
        if self.get_name() == name:
            results.append(self)
        for child in self._children:
            results.extend(child.search(name))
        return results


# =================================================================
# 示例 2: 组织架构
# =================================================================

class OrganizationComponent(abc.ABC):
    """组织组件接口"""

    @abc.abstractmethod
    def get_name(self) -> str: ...

    @abc.abstractmethod
    def get_salary(self) -> float: ...

    @abc.abstractmethod
    def display(self, indent: str = "") -> str: ...

    def get_headcount(self) -> int:
        return 0


class Employee(OrganizationComponent):
    """叶子: 员工"""

    def __init__(self, name: str, position: str, salary: float):
        self._name = name
        self._position = position
        self._salary = salary

    def get_name(self) -> str:
        return self._name

    def get_salary(self) -> float:
        return self._salary

    def get_headcount(self) -> int:
        return 1

    def display(self, indent: str = "") -> str:
        return f"{indent}👤 {self._name} ({self._position}, ¥{self._salary:.0f})"


class Department(OrganizationComponent):
    """组合节点: 部门"""

    def __init__(self, name: str):
        self._name = name
        self._members: list[OrganizationComponent] = []

    def get_name(self) -> str:
        return self._name

    def add(self, member: OrganizationComponent) -> "Department":
        self._members.append(member)
        return self

    def get_salary(self) -> float:
        total = 0.0
        for m in self._members:
            total += m.get_salary()
        return total

    def get_headcount(self) -> int:
        total = 0
        for m in self._members:
            total += m.get_headcount()
        return total

    def display(self, indent: str = "") -> str:
        lines = [
            f"{indent}🏢 {self._name}部 "
            f"(人数: {self.get_headcount()}, "
            f"薪资: ¥{self.get_salary():.0f})"
        ]
        for m in self._members:
            lines.append(m.display(indent + "  "))
        return "\n".join(lines)


# =================================================================
# 示例 3: 表达式计算
# =================================================================

class Expression(abc.ABC):
    """数学表达式接口"""

    @abc.abstractmethod
    def evaluate(self) -> float:
        """计算结果"""
        ...

    @abc.abstractmethod
    def display(self) -> str:
        """展示表达式"""
        ...


class Number(Expression):
    """叶子: 数字"""

    def __init__(self, value: float):
        self._value = value

    def evaluate(self) -> float:
        return self._value

    def display(self) -> str:
        return str(self._value)

    # 为组合提供访问
    def __repr__(self) -> str:
        return f"Number({self._value})"


class Operator(Expression):
    """组合: 二元运算表达式"""

    def __init__(self, op: str, left: Expression, right: Expression):
        self._op = op
        self._left = left
        self._right = right

    def evaluate(self) -> float:
        l = self._left.evaluate()
        r = self._right.evaluate()
        match self._op:
            case "+":   return l + r
            case "-":   return l - r
            case "*":   return l * r
            case "/":
                if r == 0:
                    raise ZeroDivisionError("除数不能为零")
                return l / r
            case "^":   return l ** r
            case _:
                raise ValueError(f"未知运算符: {self._op}")

    def display(self) -> str:
        return f"({self._left.display()} {self._op} {self._right.display()})"


# =================================================================
# 透明 vs 安全: 组合模式的两种风格
# =================================================================

# 上面展示的是"透明方式"——Component 声明了所有方法
# 下面展示"安全方式"——只在 Composite 中定义子节点管理

class SafeComponent(abc.ABC):
    """安全版本的组件接口, 不声明子节点管理方法"""

    @abc.abstractmethod
    def render(self) -> str: ...

    @abc.abstractmethod
    def count(self) -> int: ...


class SafeLeaf(SafeComponent):
    def __init__(self, label: str):
        self._label = label

    def render(self) -> str:
        return f"[叶子: {self._label}]"

    def count(self) -> int:
        return 1


class SafeComposite(SafeComponent):
    def __init__(self, label: str):
        self._label = label
        self._children: list[SafeComponent] = []

    # 子节点管理只在 Composite 中定义
    def add(self, child: SafeComponent) -> "SafeComposite":
        self._children.append(child)
        return self

    def remove(self, child: SafeComponent) -> bool:
        if child in self._children:
            self._children.remove(child)
            return True
        return False

    def render(self) -> str:
        parts = [f"[{self._label}"]
        for c in self._children:
            parts.append("  " + c.render())
        parts.append(f"[/{self._label}]")
        return "\n".join(parts)

    def count(self) -> int:
        total = 0
        for c in self._children:
            total += c.count()
        return total


# =================================================================
# 主函数
# =================================================================

def main():
    print("=" * 60)
    print("组合模式 演示")
    print("=" * 60)

    # ── 1. 文件系统 ──
    print("\n--- 1. 文件系统树 ---")

    root = Directory("root")
    home = Directory("home")
    usr = Directory("usr")
    var = Directory("var")
    logs = Directory("logs")

    root.add(home).add(usr).add(var)
    var.add(logs)

    home.add(File("README.md", 1024))
    home.add(File("config.json", 512))
    home.add(File(".gitignore", 128))

    usr.add(File("bin.tar.gz", 50_000_000))
    usr.add(Directory("share"))

    logs.add(File("sys.log", 2_048_000))
    logs.add(File("access.log", 512_000))

    print(root.display())

    # 搜索文件
    print("\n  搜索 'sys.log':")
    for found in root.search("sys.log"):
        print(f"    找到: {found.get_name()} ({found.get_size()} bytes)")

    # 递归大小
    print(f"\n  根目录总大小: {root.get_size():,} bytes")
    print(f"  /var/logs 大小: {logs.get_size():,} bytes")

    # ── 2. 组织架构 ──
    print("\n--- 2. 组织架构树 ---")

    company = Department("总公司")

    tech = Department("技术")
    sales = Department("销售")
    hr = Department("人事")

    company.add(tech).add(sales).add(hr)

    tech.add(Employee("张三", "技术总监", 50000))
    backend = Department("后端")
    frontend = Department("前端")
    tech.add(backend).add(frontend)

    backend.add(Employee("李四", "高级工程师", 35000))
    backend.add(Employee("王五", "工程师", 20000))
    backend.add(Employee("赵六", "工程师", 18000))

    frontend.add(Employee("孙七", "前端负责人", 30000))
    frontend.add(Employee("周八", "前端工程师", 15000))

    sales.add(Employee("吴九", "销售总监", 45000))
    sales.add(Employee("郑十", "销售代表", 12000))

    hr.add(Employee("冯十一", "HR 经理", 25000))

    print(company.display())
    print(f"\n  公司总人数: {company.get_headcount()}")
    print(f"  公司总薪资: ¥{company.get_salary():,.0f}")

    # ── 3. 表达式计算 ──
    print("\n--- 3. 表达式树 ---")

    # 表达式: (3 + 5) * (10 - 2) / 4
    expr = Operator(
        "/",
        Operator(
            "*",
            Operator("+", Number(3), Number(5)),
            Operator("-", Number(10), Number(2)),
        ),
        Number(4),
    )
    print(f"  表达式: {expr.display()}")
    print(f"  结果: {expr.evaluate():.0f}")

    # 更复杂的表达式: 2 ^ (1 + 3) - 10
    expr2 = Operator(
        "-",
        Operator("^", Number(2), Operator("+", Number(1), Number(3))),
        Number(10),
    )
    print(f"  表达式: {expr2.display()}")
    print(f"  结果: {expr2.evaluate():.0f}  (即 2^4 - 10)")

    # ── 4. 安全 vs 透明 ──
    print("\n--- 4. 安全方式组合 (Safe Composite) ---")
    tree = SafeComposite("root")
    tree.add(SafeLeaf("a")).add(SafeLeaf("b"))
    inner = SafeComposite("inner")
    inner.add(SafeLeaf("c")).add(SafeLeaf("d"))
    tree.add(inner)
    print(tree.render())
    print(f"  总节点数: {tree.count()}")

    print("\n" + "=" * 60)
    print("组合模式核心要点")
    print("=" * 60)
    print("  * Component: 统一接口, 叶子+组合使用相同操作")
    print("  * Leaf: 没有子节点的元素, 实现具体行为")
    print("  * Composite: 包含子节点, 委派操作")
    print("  * 透明 vs 安全: 子节点管理方法放哪?")
    print("    透明 - 在 Component 声明 (一致但可能不安全)")
    print("    安全 - 只在 Composite 定义 (安全但需类型检查)")
    print("=" * 60)


if __name__ == "__main__":
    main()
