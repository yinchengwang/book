# comprehensions 学习笔记

## 概念地图

Python 推导式是 Pythonic 的数据转换语法：

- **列表推导式**：`[x ** 2 for x in range(10)]`——替代 `map()`
- **字典推导式**：`{k: v for k, v in d.items()}`——快速构建/转换字典
- **集合推导式**：`{x for x in s}`——自动去重
- **生成器表达式**：`gen = (x ** 2 for x in range(1000000))`——惰性求值
- **vs map/filter**：推导式更直观，可读性更好

## 踩坑记录

1. **大列表内存问题**：列表推导式立即求值，大列表占用大量内存
2. **生成器只可迭代一次**：消耗后需重新创建
3. **嵌套推导式**：可读性差，优先用普通循环
4. **推导式中的副作用**：避免修改外部变量，保持纯净
5. **性能测试**：简单场景推导式快，复杂逻辑 map/filter 可能更快

## 工程对照（≥100 字硬约束）

Python 推导式在 `learning/scripts/sync-pipeline.py` 中有大量实践：

1. **列表过滤**：`[f for f in files if not f.startswith('.')]` 过滤隐藏文件
2. **字典构建**：`stats = {card: count for card, count in stats.items() if count > 0}`
3. **集合去重**：`unique_tags = {tag for tags in all_tags for tag in tags}`
4. **生成器节省内存**：`sum(x for x in large_dataset)` 避免创建中间列表
5. **嵌套循环**：`[(x, y) for x in range(10) for y in range(x)]` 组合枚举
6. **条件表达式**：`[x if x > 0 else -x for x in data]` 三元推导式

学完本卡能动手的事：用列表推导式重写 `sync-pipeline.py` 中的所有 `map()`/`filter()` 调用。
