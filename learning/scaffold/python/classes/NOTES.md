# classes 学习笔记

## 概念地图

Python 的 OOP 与 C++/Java 有显著差异：

- **一切皆对象**：类本身也是对象，`type` 是元类
- **`self` 参数**：类似 C++ 的 `this`，但必须显式声明
- **`__init__` vs `__new__`**：`__init__` 在对象创建后初始化，`__new__` 负责创建对象（很少用）
- **特殊方法**：以双下划线开头和结尾，如 `__str__`、`__add__`、`__len__`
- **属性命名约定**：`_xxx` 约定为受保护属性，`__xxx` 会触发名称改写（name mangling）
- **MRO**：多重继承时使用 C3 线性化算法，保证基类只出现一次且顺序一致

## 踩坑记录

1. **`self` 不能省略**：Python 方法的第一个参数必须是 `self`（或任意名字）
2. **可写属性问题**：`@property` 默认只读，若要 setter 用 `@xxx.setter`
3. **多重继承的复杂性**：优先使用组合而非继承，菱形继承需要理解 MRO
4. **`__repr__` vs `__str__`**：`__repr__` 用于调试（明确），`__str__` 用于用户（友好）
5. **`isinstance` vs `type`**：用 `isinstance()` 而非 `type() == X`，因为 `isinstance()` 支持继承

## 工程对照（≥100 字硬约束）

Python OOP 在 `engineering/` 的存储引擎和 `learning/scripts/sync-pipeline.py` 中有大量实践：

1. **类作为配置容器**：`sync-pipeline.py` 中虽然没有定义复杂类，但 Flask/Django 框架大量使用类来组织路由、中间件、视图
2. **`@classmethod` 替代构造器**：`Date.from_string()` 模式用于解析配置文件、JSON、CSV 等多种输入格式
3. **`@staticmethod` 工具函数**：不依赖实例状态的工具函数，如验证函数、转换函数
4. **特殊方法实现协议**：Python 的 `+`/`-`/`[]` 等运算符由特殊方法控制，如 `__add__`、`__getitem__`
5. **`__repr__` 调试友好**：所有 `engineering/src/db/` 中的 C 结构体都有 `to_string()` 方法，Python 的 `__repr__` 类似但更规范
6. **MRO 解决多重继承**：`SQLAlchemy` 的声明式模型使用多重继承实现 mixin 模式

学完本卡能动手的事：实现一个 `Fraction` 类，重写四则运算特殊方法，实现分数运算。
