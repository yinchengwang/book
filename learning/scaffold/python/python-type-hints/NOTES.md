# python-type-hints 学习笔记

## 概念地图

Python 类型提示提供静态类型检查支持：

- **TypeVar**：声明泛型变量
- **Generic**：泛型类/函数的基础
- **Protocol**：结构子类型（duck typing 的类型化版本）
- **Union**：联合类型，多种可能类型
- **Optional**：可选类型，等价于 `Union[T, None]`

## 常用类型

| 类型 | 含义 |
|------|------|
| `List[int]` | 整数列表 |
| `Dict[str, Any]` | 字符串到任意类型映射 |
| `Tuple[int, str]` | 固定长度异构元组 |
| `Callable[[int], str]` | 输入 int 返回 str 的函数 |
| `Iterable[T]` | 可迭代对象 |
| `Iterator[T]` | 迭代器 |

## mypy 静态检查

```bash
pip install mypy
mypy --strict your_code.py
```

## 工程对照（≥100 字硬约束）

类型提示在现代 Python 中广泛使用：

1. **标准库**：typing 模块提供所有类型工具
2. **FastAPI/Pydantic**：依赖类型提示做参数验证
3. **数据类**：dataclasses 与类型提示结合
4. **Protocol**：替代 ABC 做结构化类型检查
5. **IDE 支持**：PyCharm/VSCode 提供智能提示
6. **代码文档**：类型注解是最好的文档

学完本卡能动手的事：用 mypy 检查自己的 Python 代码，修复类型错误。
