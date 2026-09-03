#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Python 基础类型学习卡片

演示内容：
  - int/float/str/bytes/list/tuple/dict/set 的创建和类型
  - 类型转换（int(), str(), list(), dict() 等）
  - is vs == 的区别（重点演示 None、small int 缓存）
  - 基础运算符
"""

import sys
from typing import Dict, List, Set, Tuple

# ============================================================
# 第 1 节：基本类型创建与 type() 查看
# ============================================================

print("[types] === 基本类型创建与 type() 查看 ===\n")

# 数值类型
i = 42
f = 3.14
b = True  # bool 是 int 的子类

print("[types] int   = {}, type = {}".format(i, type(i).__name__))
print("[types] float = {}, type = {}".format(f, type(f).__name__))
print("[types] bool  = {}, type = {} (bool 是 int 子类)".format(b, type(b).__name__))

# 字符串与字节
s = "hello"
by = b"world"

print("[types] str   = '{}', type = {}".format(s, type(s).__name__))
print("[types] bytes = {}, type = {}".format(by, type(by).__name__))

# 序列类型
lst = [1, 2, 3]
tpl = (1, 2, 3)

print("[types] list  = {}, type = {}".format(lst, type(lst).__name__))
print("[types] tuple = {}, type = {}".format(tpl, type(tpl).__name__))

# 映射与集合
d = {"a": 1, "b": 2}
st = {1, 2, 3}

print("[types] dict  = {}, type = {}".format(d, type(d).__name__))
print("[types] set   = {}, type = {}".format(st, type(st).__name__))

print()

# ============================================================
# 第 2 节：类型转换
# ============================================================

print("[types] === 类型转换 ===\n")

# 字符串转换
num_str = "100"
print("[types] int('100')       = {}".format(int(num_str)))
print("[types] float('100')     = {}".format(float(num_str)))

# 数值转字符串
n = 99
print("[types] str(99)          = '{}'".format(str(n)))
print("[types] str(3.14)        = '{}'".format(str(3.14)))

# 列表/元组/集合互转
tpl = (1, 2, 3)
lst_from_tpl = list(tpl)
set_from_tpl = set(tpl)

print("[types] list((1,2,3))    = {}".format(lst_from_tpl))
print("[types] set((1,2,3,2))   = {}".format(set_from_tpl))

# 字典转换（从键值对列表）
pairs = [("x", 1), ("y", 2)]
d_from_pairs = dict(pairs)
print("[types] dict([('x',1)])  = {}".format(d_from_pairs))

# bytes 与 str 互转
raw = b"abc"
s_from_bytes = raw.decode("utf-8")
back_to_bytes = s_from_bytes.encode("utf-8")

print("[types] b'abc'.decode()  = '{}'".format(s_from_bytes))
print("[types] 'abc'.encode()   = {}".format(back_to_bytes))

print()

# ============================================================
# 第 3 节：is vs == 的区别
# ============================================================

print("[types] === is vs == 的区别 ===\n")

# None 的单例特性
a = None
b = None
print("[types] None is None     = {}".format(a is b))
print("[types] None == None     = {}".format(a == b))
print("[types] id(None)         = {} (全局唯一)".format(id(None)))

# 小整数缓存（-5 ~ 256）
small1 = 100
small2 = 100
large1 = 1000
large2 = 1000

print("")
print("[types] 100 is 100       = {}".format(small1 is small2))
print("[types] id(100)==id(100) = {}".format(id(small1) == id(small2)))
print("[types] 1000 is 1000     = {}".format(large1 is large2))
print("[types] id(1000)!=id(1k) = {}".format(id(large1) != id(large2)))
print("[types] 1000 == 1000     = {}".format(large1 == large2))

# 字符串驻留
s1 = "hello"
s2 = "hello"
s3 = "hel" + "lo"
print("")
print("[types] 'hello' is 'hello'     = {}".format(s1 is s2))
print("[types] 'hel'+'lo' is 'hello'  = {}".format(s3 is s1))

# 列表永远不等值（不同对象）
l1 = [1, 2]
l2 = [1, 2]
print("")
print("[types] [1,2] is [1,2]   = {}".format(l1 is l2))
print("[types] [1,2] == [1,2]   = {}".format(l1 == l2))

print()

# ============================================================
# 第 4 节：基础运算符
# ============================================================

print("[types] === 基础运算符 ===\n")

# 算术运算符
x = 7
y = 3
print("[types] 7 + 3   = {}   (加)".format(x + y))
print("[types] 7 - 3   = {}   (减)".format(x - y))
print("[types] 7 * 3   = {}   (乘)".format(x * y))
print("[types] 7 / 3   = {}   (除，结果总是 float)".format(x / y))
print("[types] 7 // 3  = {}   (整除)".format(x // y))
print("[types] 7 % 3   = {}   (取余)".format(x % y))
print("[types] 7 ** 3  = {}   (幂)".format(x ** y))

# 比较运算符
print("")
print("[types] 5 == 5.0 = {}   (值相等，类型不同)".format(5 == 5.0))
print("[types] 'a' < 'b' = {}   (字典序)".format('a' < 'b'))
print("[types] [1,2] < [1,3] = {}  (逐元素比较)".format([1,2] < [1,3]))

# 逻辑运算符
print("")
print("[types] True and False = {}".format(True and False))
print("[types] True or False  = {}".format(True or False))
print("[types] not True       = {}".format(not True))
default_val = '' or 'default'
print("[types] '' or 'default' = '{}'  (短路求值)".format(default_val))

# 位运算符
a_val = 0b1100  # 12
b_val = 0b1010  # 10
print("")
print("[types] 0b1100 & 0b1010 = {}   (按位与)".format(a_val & b_val))
print("[types] 0b1100 | 0b1010 = {}   (按位或)".format(a_val | b_val))
print("[types] 0b1100 ^ 0b1010 = {}   (按位异或)".format(a_val ^ b_val))
print("[types] ~0b1100          = {}    (按位取反)".format(~a_val))

# 成员运算符
lst = [1, 2, 3]
d = {"a": 1, "b": 2}
d_keys = d.keys()

print("")
print("[types] 2 in [1,2,3]        = {}".format(2 in lst))
print("[types] 'a' in dict.keys()  = {}   (dict 检查 key)".format('a' in d_keys))
print("[types] 4 not in [1,2,3]    = {}".format(4 not in lst))

print()

# ============================================================
# 第 5 节：类型注解（typing 模块）
# ============================================================

print("[types] === 类型注解（typing 模块） ===\n")

def process(data, meta):
    # type: (List[int], Dict[str, str]) -> Tuple[int, Set[str]]
    """演示类型注解的函数签名"""
    total = sum(data)
    keys = set(meta.keys())
    return total, keys

result = process([1, 2, 3], {"x": "y", "z": "w"})
print("[types] process([1,2,3], {{'x':'y'}}) = {}".format(result))

print()

# ============================================================
# 结束
# ============================================================

print("[types] === 全部演示完成 ===")
print("=== PASS ===")

sys.exit(0)
