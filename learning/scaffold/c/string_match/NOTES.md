# string_match 学习笔记

## 概念地图

字符串匹配是计算机科学最基础的操作之一——从文本编辑器 Ctrl+F 到数据库全文索引，底层都是这四种算法的变体：

- **Brute Force（朴素匹配）**：O(n·m)，对每个起始位置逐字符比较。简单到几乎没有实现 bug，但在"aaaa...aaab"中找"aaab"时退化严重（每次都匹配到最后一个字符才失败）
- **KMP（Knuth-Morris-Pratt）**：O(n+m)，核心是 lps（Longest Prefix Suffix）数组——对 pattern 做预处理，记录每个位置的最长相同前后缀长度。失配时 j 不回退到 0，而是跳到 `lps[j-1]`，保留已匹配信息。lps 构建本身也是一次 KMP 匹配（pattern 对自身）
- **Boyer-Moore**：O(n/m) 平均、O(n·m) 最坏。核心创新——**从右往左比较**，用坏字符规则（bad char）和好后缀规则（good suffix）计算安全跳转距离。本 demo 仅实现坏字符规则，生产级实现需要两个规则取最大值
- **Rabin-Karp**：O(n+m) 平均、O(n·m) 最坏（hash 碰撞）。用滚动哈希把字符串比较变成整数比较——`H(s[i+1..i+m]) = (H(s[i..i+m-1]) - s[i]·BASE^(m-1))·BASE + s[i+m]`。hash 命中后仍需逐字符确认（防止碰撞）

## 踩坑记录

1. **KMP lps 数组的含义**：`lps[i]` 是 pattern[0..i] 的"最长相同真前后缀"长度——**真前后缀**意味着不能是自身。很多人第一次实现时把 `lps[i]` 设为包含自身的前后缀长度，导致 j=lps[j-1] 时死循环
2. **BM 坏字符表的负偏移**：`shift = j - badchar[text[s+j]]` 可能 ≤0（坏字符在 pattern 中出现位置比 j 更靠后），必须 `max(1, shift)`，否则会死循环或回退
3. **RK 滚动哈希的取模负数**：`new_hash = (BASE*(hash - text[i]*h) + text[i+m]) % MOD` 中 `hash - text[i]*h` 可能是负数——C 的 `%` 对负数结果是实现定义的。必须 `if (new_hash < 0) new_hash += MOD` 或改用 `((hash - text[i]*h) % MOD + MOD) % MOD`
4. **本机无 make**：MinGW 的 Git Bash 环境没有 `make` 命令。直接用 `gcc -Wall -Wextra -O2 -std=c11 -o stringmatch_demo main.c && ./stringmatch_demo` 编译运行
5. **BM 生产实现 vs 教学简化**：本 demo 只实现了坏字符规则，生产级的 BM（如 grep -F 用的 Boyer-Moore-Horspool）只用坏字符、跳过好后缀，在实践中的表现优于完整 BM。原因是好后缀的表构建开销在短 pattern 上得不偿失

## 工程对照（≥100 字硬约束）

字符串匹配在工程侧不是论文概念——它嵌入在每一个需要"找东西"的代码路径中：

1. **中文分词的字符串扫描**：`engineering/src/algo-prod/dict/dict_cut.c` 中的中文分词器用前向最大匹配法扫描词库——本质是多次模式匹配（对每个起始位置找最长词）。KMP 的多模式扩展（Aho-Corasick 自动机）可直接用于多词同时匹配，把分词从 O(n·dict_size) 降到 O(n + total_pattern_len)
2. **BM25 全文检索的 token 匹配**：`engineering/src/db/index/vector_index/BM25/bm25_search.c` 中 BM25 评分需要统计 term frequency——把文档切词后对每个 term 做匹配计数。Rabin-Karp 的滚动哈希可用于加速多词匹配（multi-pattern RK）——一次扫描文本，多个 term 同时 hash 比较
3. **全文索引 FTS**：`engineering/src/db/index/fulltext/` 目录下的全文索引实现依赖倒排索引中的字符串匹配——每个 postings list 的 term 查找就是字符串匹配的特化（term 是固定 key，text 是文档内容）
4. **预留测试入口**：`engineering/test/algo/kmp/` 当前为空目录，后续 OPSX 变更可将 KMP 的工程实现（如 `kmp_search()` 包装函数）放入此处并配上单元测试
5. **SQL LIKE 模式匹配**：`engineering/src/db/sql/sql_exec.c` 中的 LIKE 运算符本质上是一种通配符模式匹配——`%` 和 `_` 相当于正则的 `.*` 和 `.`，KMP 的 lps 思想可以扩展到通配符匹配（globbing）

学完本卡后能动手的事：阅读 `engineering/src/algo-prod/dict/dict_cut.c` 的中文分词匹配循环，用本卡的 KMP 知识分析为什么短词词典（1000 词以下）用 BF 反而比 KMP 快——因为 KMP 的 lps 构建开销 O(m) 在短 pattern 上超过了 O(n·m) 中的常数因子优势。
