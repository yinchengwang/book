# 工程对照说明

## 与 engineering/src/db/ 的关联

### 文本搜索与分词

在 `engineering/src/db/fulltext` 模块中，文本搜索依赖字符串匹配算法：

```c
// fulltext.c 中的分词器
typedef struct {
    char *buffer;
    size_t len;
    size_t pos;
} Tokenizer;

int tokenizer_next(Tokenizer *t, char *token, size_t max_len)
{
    // 跳过空格，提取下一个单词
    while (t->pos < t->len && isspace(t->buffer[t->pos])) t->pos++;
    // ...
}
```

这与本模块的字符串处理思路一致：按规则分割字符串为可处理单元。

### 词库匹配：Trie 的工程应用

在数据库全文索引中，Trie 树用于构建倒排索引的前缀结构：

```c
// index_trie.c 中的前缀索引
typedef struct {
    int node_id;
    int posting_list[MAX_POSTINGS];
    int posting_count;
} TrieNode;

int trie_lookup_prefix(Trie *t, const char *prefix, InvertedIndex *result)
{
    int node = 0;
    // 定位前缀节点
    for (int i = 0; prefix[i]; i++) {
        int idx = prefix[i] - 'a';
        if (t->nodes[node].child[idx] == 0) return 0;
        node = t->nodes[node].child[idx];
    }
    // 收集所有后继节点的 posting list
    return collect_postings_dfs(t, node, result);
}
```

### 路由器 ACL 与字符串匹配

在网络设备中，访问控制列表（ACL）使用 Trie 结构存储规则：

```c
// acl_engine.c 中的规则匹配
typedef struct ACLNode {
    uint32_t prefix;        /* IP 前缀 */
    int prefix_len;         /* 前缀长度 */
    int action;             /* 允许/拒绝 */
    struct ACLNode *next[2]; /* 二叉树分支 */
} ACLNode;

int acl_lookup(ACLNode *root, uint32_t ip)
{
    ACLNode *node = root;
    ACLNode *match = NULL;
    for (int i = 31; i >= 0 && node; i--) {
        if (node->prefix_len == 0) match = node;  /* 默认规则 */
        node = node->next[(ip >> i) & 1];
    }
    return match ? match->action : ACL_DENY;
}
```

这本质上是二叉 Trie（基数树），是标准 Trie 的变体。

### KMP 在正则表达式引擎中的应用

数据库正则表达式匹配器使用 KMP 思想优化字符串扫描：

```c
// regex_engine.c 中的确定性有限自动机 (DFA)
typedef struct {
    int state;
    int transitions[MAX_STATES][ALPHABET_SIZE];
} RegexDFA;

int regex_match(RegexDFA *dfa, const char *text)
{
    int state = 0;
    for (int i = 0; text[i]; i++) {
        int c = (unsigned char)text[i];
        state = dfa->transitions[state][c];
        if (state < 0) return 0;  /* 死亡状态 */
    }
    return dfa->final_states[state];
}
```

### 性能优化：SIMD 加速字符串匹配

现代数据库使用 SIMD 指令加速大块文本匹配：

```c
// simd_search.c
#include <immintrin.h>

size_t simd_strstr(const char *text, size_t n, const char *pat, size_t m)
{
    __m256i p = _mm256_loadu_si256((__m256i *)pat);
    for (size_t i = 0; i < n; i += 32) {
        __m256i t = _mm256_loadu_si256((__m256i *)(text + i));
        __m256i cmp = _mm256_cmpeq_epi8(t, p);
        // ... 后续处理
    }
}
```

这比 KMP 的标量实现快 4-8 倍（在支持 AVX2 的 CPU 上）。
