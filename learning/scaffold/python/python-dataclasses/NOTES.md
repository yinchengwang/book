# python-dataclasses 学习笔记

## 概念地图

dataclasses 减少 Python 类的样板代码：

- **@dataclass**：装饰器，自动生成 __init__/__repr__/__eq__
- **field()**：定制字段行为，如 default_factory
- **__post_init__**：初始化后回调
- **frozen=True**：创建不可变对象

## dataclass vs 普通类

| 特性 | dataclass | 普通类 |
|------|-----------|--------|
| 样板代码 | 极少 | 多 |
| __init__ | 自动生成 | 手动编写 |
| __repr__ | 自动生成 | 手动编写 |
| __eq__ | 自动生成 | 手动编写 |

## 踩坑记录

1. **可变默认值**：用 field(default_factory=list) 而非 default=[]
2. **field(init=False)**：不作为参数初始化
3. **frozen**：不可变，但 hash 需要所有字段可 hash

## 工程对照（≥100 字硬约束）

dataclass 在现代 Python 中广泛应用：

1. **配置对象**：替代 namedtuple，更灵活
2. **DTO/VO**：数据传输对象
3. **Pydantic**：基于 dataclass 做验证
4. **序列化**：dataclass 与 JSON 转换
5. **数据库模型**：简化 ORM 模型
6. **事件对象**：日志事件、消息事件

学完本卡能动手的事：用 @dataclass 重构项目中的简单数据类。
