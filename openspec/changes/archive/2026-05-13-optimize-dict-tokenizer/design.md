## 总体架构

```
                          ┌─────────────────────────────┐
                          │         dict.h (API)         │
                          │  dict_cut()       ← 不变     │
                          │  dict_cut_for_search() ← 新增 │
                          │  dict_normalize_term() ← 不变 │
                          │  dict_is_stop_word()  ← 不变 │
                          │  dict_resolve_synonym() ← 不变│
                          │  dict_enable_hmm()    ← 新增 │
                          └─────────────┬───────────────┘
                                        │
          ┌─────────────────────────────┼─────────────────────────────┐
          │              dict_private.h (内部结构)                     │
          │                                                          │
          │  ┌─────────────────┐  ┌──────────────┐  ┌─────────────┐  │
          │  │  Trie + DP 分词  │  │  HMM 解码器   │  │ 哈希查找表   │  │
          │  │  (dict_cut.c)   │  │ (dict_hmm.c) │  │ (dict_core) │  │
          │  │                 │  │              │  │             │  │
          │  │ • 最优路径 DP   │  │ • Viterbi    │  │ • 停用词    │  │
          │  │ • CutForSearch  │  │ • 预置模型   │  │ • 同义词    │  │
          │  │ • HMM OOV 回退  │  │ • 发射/转移  │  │ • FNV-1a   │  │
          │  └────────┬────────┘  └──────┬───────┘  └──────┬──────┘  │
          │           │                  │                   │         │
          │           └──────────────────┼───────────────────┘         │
          │                              │                             │
          │  ┌───────────────────────────┴──────────────────────────┐  │
          │  │              dict_node_t (Trie 节点)                  │  │
          │  │                                                      │  │
          │  │  ┌─────────────────┐    ┌─────────────────────────┐  │  │
          │  │  │ Level 0 (root)  │    │ Level ≥ 1               │  │  │
          │  │  │ children[65536] │    │ uthash (dict_edge_t)    │  │  │
          │  │  │ O(1) 直接索引   │    │ O(1) 均摊查找          │  │  │
          │  │  └─────────────────┘    └─────────────────────────┘  │  │
          │  └──────────────────────────────────────────────────────┘  │
          └──────────────────────────────────────────────────────────┘
```

## 子模块设计

### 1. CutForSearch — `dict_cut_for_search()`

**算法**：在现有 DP 最优路径 `build_route()` 生成的最佳路径上，回溯时对每个已匹配词额外检查其前缀子串是否也是 Trie 中的词，若是则作为附加 token 输出。

```
例："南京市长江大桥"
  Cut:          ["南京市", "长江大桥"]
  CutForSearch: ["南京", "南京市", "市长", "长江", "长江大桥", "大桥"]
                （附加的子词用链式回溯方式产生）
```

**输出格式**：沿用 `token_t` 结构，token 按文本顺序排列（原始词 + 子词交错）。

**内部实现**：
- 复用 `dict_build_route()` 产生的 DP 路径
- 新增 `dict_expand_subwords()` 函数，沿最优路径对每个已匹配位置做 Trie 子词枚举
- 调用方通过 `dict_cut_for_search()` 入口，签名与 `dict_cut()` 完全一致

### 2. 哈希查找 — 停用词/同义词

**数据结构变更**：

```c
// 旧：链表
typedef struct dict_stop_entry_t {
    char *word;
    struct dict_stop_entry_t *next;  // O(n) 遍历
} dict_stop_entry_t;

// 新：拉链哈希表
typedef struct dict_hash_bucket_t {
    char *word;
    uint64_t hash_value;
    struct dict_hash_bucket_t *next;
} dict_hash_bucket_t;

typedef struct dict_hash_set_t {
    dict_hash_bucket_t **buckets;
    int32_t bucket_count;
    int32_t item_count;
} dict_hash_set_t;
```

**哈希函数**：复用现有的 `dict_hash_fnv1a()`（与 BM25 的 `bm25_hash_term` 同款 FNV-1a 64-bit）。

**扩容策略**：负载因子 > 2.0 时扩容为 2×，rehash。初始桶数 64。

**接口不变**：`dict_is_stop_word()`、`dict_resolve_synonym()`、`dict_add_stop_word()`、`dict_add_synonym()` 签名全部不变。

### 3. HMM 未登录词识别

**算法**：经典 Viterbi 解码，在 Trie 无法匹配的连续 CJK 片段上执行。

**状态集合**：4 状态 — B（词首）、E（词尾）、M（词中）、S（单字成词）。

**模型数据**（嵌入式）：
- 初始概率 π：`{B: 0.6, E: 0.0, M: 0.0, S: 0.4}`
- 转移概率 A：B→E 为主，M→E 次之，E→B/S 后继
- 发射概率 B：每个 Unicode 码点在各状态下的条件概率

预置模型数据来自 jieba HMM 模型（prob_start.py、prob_trans.py、prob_emit.py），编译为静态 `const` 数组嵌入代码。

**集成点**：
- `dict_collect_runes()` 产生 rune 数组后，先尝试 Trie DP，若连续 N 个字符 Trie 无任何匹配路径，将该段标记为 OOV
- OOV 片段由 `dict_hmm_viterbi()` 处理，产生 B/M/E/S 标签序列
- 标签序列切割为 token：B 开始 E 结束为一个词，S 独立成词

**启用方式**：
- `dict_enable_hmm(dict, true)` — 开关
- `dict_set_hmm_model(dict, ...)` — 自定义模型（可选，默认用嵌入式模型）

### 4. Trie 顶层数组化

**观察**：Unicode CJK 统一汉字基本块在 U+4E00–U+9FFF，常用汉字约 20,000 个。Trie 根节点的出边数量大，uthash 每次 HASH_FIND 都要计算哈希 + 比较。

**方案**：根节点（level 0）使用直接索引数组：

```c
#define DICT_TRIE_TOP_ARRAY_SIZE 65536  // BMP 范围

typedef struct dict_node_t {
    bool is_word;
    int32_t freq;
    // Level 0: 固定数组
    struct dict_node_t **top_children;  // 仅 root 使用，65536 槽位
    // Level ≥ 1: uthash
    dict_edge_t *edges;                 // uthash 句柄
} dict_node_t;
```

**代价**：root 节点占用 65536 × 8 = 512KB（64位指针），但只需一次分配。如果不接受全量 BMP 数组，可缩减为仅 CJK 区间 [0x4E00, 0x9FFF] ≈ 21K 槽位 + ASCII 区间 [0x00, 0x7F] 128 槽位。

**增量修改**：`dict_find_edge()` 检测 `node->top_children != NULL` 则直接数组索引；否则走 uthash 路径。

## BM25 接口兼容性

```
BM25 使用的 dict 接口          本次变更

dict_cut()                    签名不变，内部集成 HMM OOV 回退
dict_normalize_term()         签名不变，内部哈希查找提速
dict_free_tokens()            不变
dict_create_default()         不变，内部初始化哈希表 + HMM 模型
dict_drop()                   不变，内部释放新数据结构
dict_t *                      不变，struct 内部扩展字段对外不可见
```

BM25 源码 **零修改**，重新编译即可获得所有优化效果。后续若想启用 CutForSearch 提升召回，只需将 `dict_cut()` 调用改为 `dict_cut_for_search()`（一行改动）。

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| HMM 模型数据过大导致二进制膨胀 | 使用 compact 格式 + 编译期量化（概率值用 uint16 存 0–65535） |
| 顶层数组 512KB 内存开销 | 提供编译宏 `DICT_TRIE_TOP_ARRAY_SIZE` 可配置 |
| CutForSearch 输出 token 数量膨胀 | 调用方可选；输出 token 总数有上界（原文长度 × 平均子词数） |
