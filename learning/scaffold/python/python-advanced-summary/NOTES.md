# python-advanced-summary 学习笔记

## 概念地图

Python 进阶主题全景：

| 主题 | 核心概念 | 应用场景 |
|------|----------|----------|
| 装饰器 | @decorator + wraps | 日志、计时、缓存 |
| 生成器 | yield + 惰性求值 | 大数据处理 |
| 上下文管理器 | with + __enter__ | 资源管理 |
| 元类 | type + __new__ | ORM、注册表 |
| 闭包 | nonlocal + 工厂 | 私有状态 |
| asyncio | async/await | 高并发 I/O |
| threading | Lock + Event | I/O 密集并发 |
| multiprocessing | Process + Pool | CPU 密集并行 |
| 类型提示 | TypeVar + Generic | 代码文档、IDE |
| 描述符 | __get__/__set__ | 属性验证 |
| __slots__ | 内存优化 | 高频对象 |
| ABC | abstractmethod | 接口定义 |
| dataclass | 自动生成代码 | 数据对象 |

## 学习路径

1. **基础**：装饰器、生成器、上下文管理器
2. **进阶**：元类、闭包、描述符
3. **并发**：asyncio、threading、multiprocessing
4. **工程**：类型提示、ABC、dataclass、__slots__

## 工程对照（≥100 字硬约束）

这些主题在实际项目中组合使用：

1. **Web 框架**：装饰器做路由，ABC 定义接口
2. **ORM**：元类收集字段，dataclass 表示记录
3. **异步框架**：asyncio + context manager
4. **CLI 工具**：argparse + dataclass 配置
5. **数据处理**：生成器 + multiprocessing
6. **测试**：unittest + mock + fixture

学完本卡能动手的事：综合运用各主题，编写一个功能完整的 CLI 工具。
