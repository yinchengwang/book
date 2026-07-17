# python-abc 学习笔记

## 概念地图

ABC（Abstract Base Classes）定义接口规范：

- **ABC**：抽象基类基类
- **@abstractmethod**：抽象方法，必须由派生类实现
- **抽象属性**：`@property` + `@abstractmethod`
- **register()**：注册虚拟子类，无需继承

## ABC vs Protocol

| 特性 | ABC | Protocol |
|------|-----|----------|
| 继承 | 需要显式继承 | 隐式实现 |
| 方法 | 必须 @abstractmethod | 只需签名匹配 |
| 运行时检查 | isinstance() | 不支持 |

## 踩坑记录

1. **不能实例化**：ABC 本身不能实例化
2. **部分实现**：可以有带实现的非抽象方法
3. **register vs 继承**：register 的子类可能不完全兼容

## 工程对照（≥100 字硬约束）

ABC 在 Python 框架中广泛应用：

1. **collections.abc**：Iterable, Sequence, Mapping 等
2. **Django**：Model 抽象数据库接口
3. **unittest**：TestCase 基类
4. **http**：HTTP handler 基类
5. **typing**：Protocol 替代 ABC 做结构检查
6. **插件系统**：用 ABC 定义插件接口

学完本卡能动手的事：为自己的项目定义抽象基类，规范模块接口。
