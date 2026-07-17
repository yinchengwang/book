## ADDED: dict-cut-for-search

### 概述

在现有 `dict_cut()` 最优路径分词基础上，新增 `dict_cut_for_search()` 接口，产生子词扩展 token，提升 BM25 等文本检索的召回率。

### API

```c
int32_t dict_cut_for_search(const dict_t *dict, const char *text,
                            token_t **tokens, int32_t *token_count);
```

签名与 `dict_cut()` 完全一致，行为差异仅在输出的 token 集合更大（包含子词）。

### 行为规范

- **输入**：UTF-8 编码的原始文本，与 `dict_cut()` 相同
- **输出**：`token_t` 数组，按文本字节偏移升序排列
- **Token 集合**：包含原始文本的所有 Trie 匹配词，以及最优路径上每个位置的 Trie 子词
- **空输入**：`text` 为 NULL 或空字符串时，`*tokens = NULL`，`*token_count = 0`，返回 0
- **调用方负责释放**：必须调用 `dict_free_tokens()` 释放

### 子词扩展算法

```
设 DP 最优路径为词序列 W = [w0, w1, ..., wk]，每个 wi 对应 Trie 中的一个词

对每个 wi：
  1. 输出 wi 自身
  2. 枚举 wi 在 Trie 中的所有前缀子串 s ⊂ wi，若 s 也是 Trie 中的词且 s ≠ wi，输出 s
  3. 枚举 wi 在 Trie 中的所有后缀子串 s ⊂ wi，若 s 也是 Trie 中的词且 s 尚未输出，输出 s
```

### 举例

| 输入 | dict_cut() | dict_cut_for_search() |
|------|------------|-----------------------|
| `"南京市长江大桥"` | `["南京市", "长江大桥"]` | `["南京", "南京市", "市长", "长江", "长江大桥", "大桥"]` |
| `"我来到北京清华大学"` | `["我", "来到", "北京", "清华大学"]` | `["我", "来到", "北京", "清华", "清华大学", "大学"]` |

### 约束

- `dict` 不能为 NULL
- 返回 0 表示成功，非 0 表示失败（内存不足等）
- 子词扩展不产生重复 token（同一 byte_start + byte_length 组合最多出现一次）
- 与 `dict_cut()` 共享同一个 Trie 和 HMM 配置
