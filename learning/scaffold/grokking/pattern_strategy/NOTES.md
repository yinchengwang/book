# 策略模式 — 工程对比笔记

## 关键概念

| 概念 | 本例 | 数据库工程对照 |
|------|------|---------------|
| 策略接口 | PaymentStrategy | storage_engine_t 虚函数表 |
| 具体策略 | CreditCardPayment / AlipayPayment / ... | kv_engine / vector_engine / ts_engine / ... |
| 上下文 | PaymentContext / check() | db_open(type) → 多态存储引擎 |
| 运行时切换 | context.strategy = new_strategy | SET enable_seqscan = on/off |

## 工程对照

### 1. 多模态存储引擎 (engineering/include/db/mm_storage.h)

数据库的多模态存储引擎是策略模式的直接体现：
- `storage_engine_t` 接口声明了 `open()` / `close()` / `insert()` / `query()` 等操作，相当于 Strategy
- 每种引擎（KV / Vector / Timeseries / Document / Spatial / Graph）都是一个 ConcreteStrategy
- `mm_storage_manager` 充当 Context，根据表类型选择正确的引擎
- 新增一种存储引擎无需修改已有引擎代码，只需实现 `storage_engine_t` 接口并在管理器注册

### 2. 排序算法 (engineering/src/ds/sort/)

排序模块在运行时根据数据特征选择算法：
- qsort() 通用排序 vs 计数排序（小范围整数）vs 基数排序（字符串）
- 策略模式让排序算法可互换，客户端只需调用 `sort(data, strategy)`
- 对比硬编码 if-else：每新增一种排序算法都要修改分发逻辑

### 3. GUC 参数

PostgreSQL 风格配置参数影响查询计划器的策略选择：
- `enable_seqscan = on/off` — 是否允许顺序扫描
- `enable_indexscan = on/off` — 是否允许索引扫描
- 查询计划器根据这些参数从多种扫描策略中选择最优方案
- 相当于在运行时通过配置参数切换策略

## 策略模式 vs if-else

| 维度 | 策略模式 | if-else |
|------|---------|--------|
| 可扩展性 | 新增类，不修改现有代码 | 修改函数体，风险高 |
| 复用性 | 策略可独立测试和复用 | 逻辑耦合在条件分支中 |
| 可读性 | 每个策略职责清晰 | 分支越多越难读 |
| 运行时切换 | 支持（setter 注入） | 不支持（编译时决定） |
| 适用场景 | 算法/行为频繁变化 | 条件少且固定 |

## 面试要点

1. **策略模式 vs 状态模式**：策略模式客户端主动选择算法；状态模式对象内部自动切换行为。前者对外暴露策略替换能力，后者对内封装状态转移。
2. **策略模式的实现方式**：Python 可以用函数/类作为策略，C 语言用函数指针 + 策略表，C++ 用虚函数或 std::function。
3. **策略注册表**：可以结合工厂模式，通过名称查找策略实例（如 GUC 参数到策略的映射）。
4. **策略的线程安全**：无状态策略可以共享（单例），有状态策略每次使用时需要新实例。
