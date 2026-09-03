# basic_types 学习笔记

## 概念地图

Python 的数据类型系统以"一切皆对象"为核心：

- **不可变类型**：int、float、str、bool、tuple——修改时创建新对象，**用于字典键和集合元素**
- **可变类型**：list、dict、set——原地修改，影响所有引用
- **NoneType**：单例 `None`，表示"无值"或"未初始化"
- **小整数池**：CPython 预分配 -5~256 整数对象，**同一数值的 `is` 比较为 True**
- **类型转换**：显式转换 `int()`/`float()`/`str()`/`bool()`，隐式转换发生在运算时

## 踩坑记录

1. **`int(3.7)` 截断而非四舍五入**：需四舍五入用 `round(3.7)` 或 `int(3.7 + 0.5)`
2. **`bool([])` vs `bool([0])`**：空容器/空字符串/0/None 都是假值，非空容器和非零数字是真值
3. **`lst2 = lst1` 是引用共享**：修改 `lst1` 会影响 `lst2`，需用 `lst2 = lst1.copy()` 或 `lst2 = lst1[:]`
4. **字典键必须是不可变类型**：`{[1,2]: "x"}` 报错，因为 list 是可变类型
5. **浮点数精度问题**：`0.1 + 0.2 != 0.3`（二进制浮点精度），金融计算用 `decimal.Decimal`

## 工程对照（≥100 字硬约束）

Python 数据类型在 `learning/scripts/sync-pipeline.py` 和 `engineering/` 中有大量实践：

1. **类型注解（Type Hint）**：`learning/scripts/sync-pipeline.py` 大量使用 `dict[str, Any]`、`list[str]` 等类型注解，提高代码可读性和 IDE 支持
2. **dict 作为配置容器**：`sync-pipeline.py` 的 `CONFIG: dict[str, Any]` 用字典存储 JSON 配置，`get(key, default)` 安全取值，避免 KeyError
3. **list 遍历与切片**：`sync-pipeline.py` 中 `for path in changed_files:`、`files = paths[:100]`（前100个文件），体现 list 的有序可迭代特性
4. **set 用于去重**：`seen_files: set[str] = set()` 追踪已处理文件，自动去重
5. **tuple 作为不可变序列**：函数多返回值 `return success, message` 就是 tuple，**不可变保证接口稳定**
6. **bool 用于条件判断**：所有 `if result:` 判断依赖 truthiness 规则，`if items:` 比 `if len(items) > 0:` 更 Pythonic

学完本卡能动手的事：在自己的 Python 脚本中为函数参数和返回值添加类型注解，用 `mypy --strict` 做静态检查。
