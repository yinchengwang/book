# Python 基础类型 — NOTES

## 工程对照

本节对照 `learning/scripts/sync-pipeline.py` 与 `engineering/src/` 中类似的类型使用模式，帮助理解 Python 类型系统与 C 代码的异同。

---

## 对照 1：dict 推导与 C 哈希表初始化

`sync-pipeline.py` 中大量使用 dict 字面量和 dict 推导：

```python
# sync-pipeline.py 第 139-145 行
groups = {}
for e in entries:
    s = e["stack"]
    if s not in groups:
        groups[s] = {"total": 0, "done": 0, "in_progress": 0, "todo": 0, "hard_total": 0, "hard_done": 0}
    g = groups[s]
    g["total"] += 1
```

对应 C 代码参考 `engineering/src/algo-prod/dict/dict_core.c` 中的哈希集合初始化：

```c
// dict_core.c 第 25-31 行
#define DICT_HASH_INITIAL_BUCKETS 64

void dict_hash_set_init(dict_hash_set_t *set)
{
    if (!set) return;
    set->buckets = (dict_hash_bucket_t **)calloc(DICT_HASH_INITIAL_BUCKETS,
                                                  sizeof(dict_hash_bucket_t *));
    set->bucket_count = set->buckets ? DICT_HASH_INITIAL_BUCKETS : 0;
    set->item_count = 0;
}
```

**对照要点**：
- Python dict 是动态哈希表，自动扩容；C 需要手动 `calloc` 桶数组
- Python 用 `{"total": 0, "done": 0}` 初始化嵌套结构；C 用结构体和 `memset(0)` 清零
- Python 的 `groups[s] += 1` 依赖 dict 的 `__missing__` 机制；C 需要显式检查 `if (s not in groups)` 再插入

---

## 对照 2：dataclass 与 C 结构体

`sync-pipeline.py` 第 108-114 行生成 JSON 时使用了嵌套 dict：

```python
inventory = {
    "generated_at": datetime.now(timezone.utc).isoformat(),
    "total_notes": len(entries),
    "notes": entries,
}
```

若用 dataclass 改写，类型更清晰：

```python
from dataclasses import dataclass, asdict
from typing import List
from datetime import datetime

@dataclass
class NoteEntry:
    path: str
    title: str
    stack: str
    difficulty: str = "medium"
    status: str = "todo"

# 使用 dataclass 而非裸 dict
entry = NoteEntry(path="py/types.md", title="Python 类型", stack="py")
inventory = {"generated_at": "...", "notes": [asdict(entry)]}
```

对应 C 代码参考 `engineering/src/db/core/guc.c` 中的配置参数结构体：

```c
// guc.c 第 33-34 行
static guc_param_t g_guc_params[MAX_GUC_PARAMS];
static int g_num_params = 0;
```

```c
// guc.h 中的结构体定义（推测）
typedef struct {
    char name[GUC_NAME_LEN];
    char value[GUC_VALUE_LEN];
    int type;      // GUC_TYPE_INT / GUC_TYPE_STRING / ...
    bool dynamic;
} guc_param_t;
```

**对照要点**：
- Python dataclass 用 `@dataclass` 自动生成 `__init__`、`__repr__`、`__eq__`；C 需要手写结构体和初始化函数
- Python 的 `asdict()` 将 dataclass 转为 dict 用于 JSON 序列化；C 需要手动 `snprintf` 拼接或使用 cJSON 库
- Python 类型注解（`path: str`）在运行时通过 `__annotations__` 可访问；C 类型信息在编译后丢失

---

## 对照 3：类型注解与 C 类型系统

`sync-pipeline.py` 使用了完整的类型注解：

```python
# 第 42 行
def parse_frontmatter(text: str) -> dict:
    ...

# 第 59 行
def scan_notes(base_dir: Path) -> list[dict]:
    ...
```

`main.py` 中演示的 typing 模块用法：

```python
from typing import Dict, List, Set, Tuple

def process(data: List[int], meta: Dict[str, str]) -> Tuple[int, Set[str]]:
    total: int = sum(data)
    keys: Set[str] = set(meta.keys())
    return total, keys
```

对应 C 代码参考 `engineering/src/db/index/btree/btree_core.c` 中的函数签名：

```c
// btree_core.c 第 49-56 行（推测）
static guc_param_t *find_param(const char *name) {
    for (int i = 0; i < g_num_params; i++) {
        if (strcasecmp(g_guc_params[i].name, name) == 0) {
            return &g_guc_params[i];
        }
    }
    return NULL;
}
```

**对照要点**：
- Python 类型注解是可选的、给 IDE 和 mypy 用的提示；C 类型是强制的，编译器据此做类型检查
- Python `Dict[str, str]` 对应 C 的 `struct { char *key; char *value; } *` 或 `khash_t(str)` 映射
- Python 返回 `Tuple[int, Set[str]]` 可携带多类型结果；C 需要返回结构体指针或输出参数
- `Optional[xxx]`（Python）对应 C 的 `xxx *`（可 NULL）；`None` 对应 C 的 `NULL`

---

## 对照 4：None 单例与 C NULL

`sync-pipeline.py` 中大量使用 `None` 作为哨兵值：

```python
# 第 46 行
if not m:
    return {}
```

`main.py` 演示了 `None` 的单例特性：

```python
a = None
b = None
print(a is b)  # True — 全局唯一对象
```

对应 C 代码参考 `guc.c` 第 49-56 行的 `find_param` 返回 NULL 模式：

```c
static guc_param_t *find_param(const char *name) {
    for (int i = 0; i < g_num_params; i++) {
        if (strcasecmp(g_guc_params[i].name, name) == 0) {
            return &g_guc_params[i];  // 找到则返回指针
        }
    }
    return NULL;  // 找不到返回 NULL（类似 Python 的 None）
}
```

**对照要点**：
- Python `None` 是对象，单例且可比较；C `NULL` 是宏，通常定义为 `((void *)0)`
- Python 用 `if x is None:` 判断；C 用 `if (ptr == NULL)` 判断
- `None is None` 为 True（同一对象）；`NULL == NULL` 在 C 中亦为 True（但比较的是指针值）

---

## 小结

| Python 特性 | C 对应实现 | 关键差异 |
|------------|-----------|---------|
| dict 动态哈希表 | `dict_hash_set_t` + 手动扩容 | Python 自动扩容，C 需手动管理 |
| dataclass | struct + 初始化函数 | Python 自动生成方法，C 需手写 |
| 类型注解 (hints) | C 类型声明 | Python 可选，C 强制 |
| None 单例 | NULL 指针 | Python 是对象，C 是宏 |
| is 身份比较 | 指针比较 `&a == &b` | 语义相似，但 Python 还有缓存机制 |
