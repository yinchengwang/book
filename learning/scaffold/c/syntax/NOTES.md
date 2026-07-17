# syntax 学习笔记

## 概念地图

C 语言的数据类型系统是"由小到大、由整到浮、由精确到扩展"的层级结构：

- **整数类型四件套**：`char`/`short`/`int`/`long`/`long long`，按字节数从 1 到 8 递增。**它们至少要达到指定的最小宽度，但实现可自行放大**——`int` 至少 16 位，实际几乎所有平台都是 32 位
- **`<stdint.h>` 固定宽度类型**：`int8_t`/`int16_t`/`int32_t`/`int64_t`/`uint8_t`/...——这是跨平台 C 编程的**第一条规则**：永远不用 `int`/`long` 来存储需要确定宽度的数据（Windows `long` 是 4 字节，Linux/macOS 64 位上是 8 字节）
- **运算符优先级**：15 级，最容易出错的是位运算（`&`/`|`/`^`）与比较运算（`==`/`<`）混用——`flags & MASK == 0` 永远要写 `(flags & MASK) == 0`
- **整数边界**：`INT_MAX` = 2³¹-1（~2.1 亿），`UINT_MAX` = 2³²-1。有符号溢出是 UB（未定义行为），无符号溢出环绕是标准定义的
- **类型转换**：C 允许隐式转换（int → double 精度提升、double → int 截断），但**指针类型转换要小心**——`void*` 是唯一可隐式转换为其他指针的类型

## 踩坑记录

1. **`sizeof` 在编译期求值**：`sizeof(int)` 不是函数调用，是编译期常量。`sizeof arr`（数组）给出数组总字节数，`sizeof arr / sizeof arr[0]` 才是元素个数
2. **`size_t` 是无符号整数**：用 `%zu`（C99）或 `%lu` 强制转换打印。**不要和有符号整数比较**——`(int)x - 1` 当 `x = 0` 时变成 `UINT_MAX`
3. **`INT_MAX` vs `UINT_MAX`**：`INT_MAX + 1` 在有符号下是 UB，无符号下是 `0`。BM25 索引用 `INT_MAX` 作为"无匹配"哨兵，**只能用有符号类型**
4. **优先级坑**：`a == b & c` 等价于 `a == (b & c)`，`a && b == c` 等价于 `a && (b == c)`。**永远多写括号**
5. **`int32_t` 不一定存在**：标准说"如果实现支持 32 位整数则提供"。**嵌入式平台可能没有**，此时需要 fallback

## 工程对照（≥100 字硬约束）

C 数据类型与运算符在本仓库 `engineering/` 中有大量实践：

1. **`<stdint.h>` 固定宽度类型贯穿整个工程**：`grep -rn "int32_t\|uint64_t" engineering/src/ | wc -l` 可统计——所有序列化、磁盘 I/O、网络协议层都用固定宽度类型，杜绝平台差异。例如 `engineering/rag/src/rag/persist/hnsw_persist.c` 中序列化向量 ID 用 `uint64_t`，绝不写 `long`
2. **`INT_MAX` 作为 BM25 哨兵值**：`engineering/src/db/index/vector_index/BM25/bm25_search.c` 多处用 `INT_MAX` 表示"无匹配"或"列表末尾"（行 435、454、459、466、476）。这是无符号类型用不了的典型场景——哨兵值必须区别于所有合法值
3. **运算符优先级的真实陷阱**：`engineering/src/db/index/vector_index/BM25/bm25_search.c` 行 465 的 `state->slot->posting_filters[filter_index].last_doc_id == INT_MAX - 1` 如果写成 `last_doc_id == INT_MAX - 1` 而漏掉括号就会出 bug——但工程代码普遍防御性地加括号
4. **`size_t` 数组遍历**：`engineering/src/db/view/view.c` 行 67 的 `for (int i = 0; i < mgr->view_count; i++)` 用有符号 `int`，但 `engineering/src/db/validator/sql_semantic.c` 行 64 用 `for (size_t i = 0; ...)`——**两者都对，关键是与被比较的变量类型匹配**：`view_count` 是 `int` 就用 `int`，`num_columns` 是 `size_t` 就用 `size_t`
5. **隐式转换在 `snprintf` 中**：所有 `engineering/src/db/validator/*.c` 的 `snprintf(key, sizeof(key), "table:%s:meta", table_name)` 都依赖 `sizeof(key)` 是 `size_t`，而 `%zu` 格式化；C 标准库函数的 size 参数**统一为 `size_t`**

学完本卡能动手的事：扫描 `learning/scaffold/` 下所有 main.c，把所有 `int`/`long` 改为 `int32_t`/`int64_t`（C99 兼容），统一打印格式 `%zu` for `size_t`。