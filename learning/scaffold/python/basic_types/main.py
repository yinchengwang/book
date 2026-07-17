#!/usr/bin/env python3
# basic_types scaffold — Python 基础类型
#
# 复现命令：
#   python3 main.py
#
# 演示 6 段：
#   [types]   — 基础类型 int/float/str/bool
#   [compound] — 容器类型 list/dict/set/tuple
#   [none]    — NoneType 与空值
#   [cast]    — 类型转换 int()/float()/str()/bool()
#   [id_type] — id() 和 type() 内省
#   [mutable] — 可变 vs 不可变对象

def main():
    # === [types] 基础类型 ===
    print("[types] === Python 基础类型 ===")

    # 整数
    i: int = 42
    print(f"  int    : {i} (type={type(i).__name__})")

    # 浮点数
    f: float = 3.14
    print(f"  float  : {f} (type={type(f).__name__})")

    # 字符串
    s: str = "Hello, Python!"
    print(f"  str    : '{s}' (type={type(s).__name__})")

    # 布尔值
    b: bool = True
    print(f"  bool   : {b} (type={type(b).__name__})")

    # === [compound] 容器类型 ===
    print("\n[compound] === 容器类型 ===")

    # 列表（可变）
    lst: list = [1, 2, 3, "a", "b"]
    print(f"  list   : {lst}")

    # 字典（可变，键值对）
    d: dict = {"name": "Alice", "age": 30, "city": "Beijing"}
    print(f"  dict   : {d}")

    # 集合（可变，无序唯一）
    s: set = {1, 2, 3, 2, 1}  # 去重
    print(f"  set    : {s} (自动去重)")

    # 元组（不可变）
    t: tuple = (1, 2, 3, "x")
    print(f"  tuple  : {t} (不可变)")

    # === [none] NoneType ===
    print("\n[none] === NoneType ===")

    empty_val = None
    print(f"  None   : {empty_val} (type={type(empty_val).__name__})")
    print(f"  None 是假值: {bool(None)}")

    # === [cast] 类型转换 ===
    print("\n[cast] === 类型转换 ===")

    # int -> float
    f1: float = float(42)
    print(f"  int(42)    -> float : {f1}")

    # float -> int (截断)
    i1: int = int(3.7)
    print(f"  int(3.7)   -> int   : {i1} (截断小数)")

    # str -> int
    i2: int = int("123")
    print(f"  int('123') -> int   : {i2}")

    # int -> str
    s1: str = str(42)
    print(f"  str(42)    -> str   : '{s1}'")

    # bool 转换规则
    print(f"  bool(0)    -> {bool(0)}")
    print(f"  bool(1)    -> {bool(1)}")
    print(f"  bool('')   -> {bool('')}")
    print(f"  bool('x')  -> {bool('x')}")
    print(f"  bool([])   -> {bool([])}")
    print(f"  bool([1])  -> {bool([1])}")

    # === [id_type] 内省 ===
    print("\n[id_type] === id() 和 type() ===")

    a = 1000
    b = 1000
    # 小整数池：-5~256 共享同一对象
    c = 256
    d = 256

    print(f"  a=1000, b=1000:")
    print(f"    id(a)={id(a)}, id(b)={id(b)}")
    print(f"    a is b: {a is b}")  # False（超出小整数池）

    print(f"  c=256, d=256（小整数池）:")
    print(f"    id(c)={id(c)}, id(d)={id(d)}")
    print(f"    c is d: {c is d}")  # True（池内共享）

    print(f"  type(42)   : {type(42)}")
    print(f"  type('x')  : {type('x')}")

    # === [mutable] 可变 vs 不可变 ===
    print("\n[mutable] === 可变 vs 不可变 ===")

    # 不可变：int, str, tuple
    x = 42
    y = x
    x = 100  # 创建新对象
    print(f"  int 不可变: x={x}, y={y}")

    # 可变：list, dict, set
    lst1 = [1, 2, 3]
    lst2 = lst1  # 共享引用
    lst1.append(4)  # 原地修改
    print(f"  list 可变: lst1={lst1}, lst2={lst2}")
    print(f"    lst1 is lst2: {lst1 is lst2}")

    # 避免共享：用 copy 或切片
    lst3 = [1, 2, 3]
    lst4 = lst3.copy()  # 或 lst3[:]
    lst3.append(4)
    print(f"  copy 后: lst3={lst3}, lst4={lst4}")

    print("\n=== PASS ===")

if __name__ == "__main__":
    main()
