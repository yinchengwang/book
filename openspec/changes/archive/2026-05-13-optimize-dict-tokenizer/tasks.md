## Phase 1：哈希查找替换（基础优化）

- [x] `dict_private.h`：新增 `dict_hash_set_t`/`dict_hash_map_t` 结构体；替换 `dict_t` 中 `stop_word_head`/`synonym_head` 为哈希表字段
- [x] `dict_core.c`：实现 `dict_hash_set_init/destroy/add/contains` 和 `dict_hash_map_init/destroy/put/get` 内部函数；改写 `dict_add_stop_word()`/`dict_add_synonym()` 为哈希插入；改写 `dict_find_stop_entry()`/`dict_find_synonym_entry()` 为哈希查找；`dict_create()` 中初始化哈希表；`dict_reset()`/`dict_drop()` 中释放哈希表
- [x] 测试：添加停用词/同义词哈希查找正确性测试（大数量 + 碰撞场景）

## Phase 2：Trie 顶层数组化（性能优化）

- [x] `dict_private.h`：`dict_node_t` 新增 `top_children` 指针字段；定义 `DICT_TRIE_TOP_SIZE` 宏
- [x] `dict_core.c`：`dict_node_create()` 的 root 节点分配 top_children 数组；`dict_find_edge()` 增加数组分支；`dict_get_or_create_edge()` 适配数组分支；`dict_drop()` 递归释放时适配
- [x] 测试：验证大规模词典（2000+ 词）的 Trie 构建和查询正确性

## Phase 3：CutForSearch 分词模式（召回增强）

- [x] `dict_private.h`：声明 `dict_cut_for_search()` 内部函数；新增 `dict_expand_subwords()` 声明
- [x] `dict_cut.c`：实现 `dict_expand_subwords()` — 沿 DP 最佳路径回溯，枚举 Trie 子词；实现 `dict_cut_for_search()` — 复用 `dict_collect_runes()` + `dict_build_route()`，输出时调用 `dict_expand_subwords()`
- [x] `dict.h`：新增 `int32_t dict_cut_for_search(const dict_t *dict, const char *text, token_t **tokens, int32_t *token_count)` 声明
- [x] 测试：对比 `dict_cut()` vs `dict_cut_for_search()` 输出差异；验证子词均在 Trie 中

## Phase 4：HMM 未登录词识别（鲁棒性增强）

- [x] `dict_hmm_model.c`：嵌入 HMM 初始概率、转移概率、发射概率数据（compact uint16 格式）
- [x] `dict_hmm.c`：实现 `dict_hmm_viterbi()` — 对 rune 数组执行 4 状态 Viterbi 解码；实现 `dict_hmm_segment()` — 将标签序列切割为 token
- [x] `dict_private.h`：新增 HMM 状态/模型结构体；`dict_t` 新增 `hmm_enabled`/`hmm_model` 字段
- [x] `dict_core.c`：`dict_create()` 初始化 HMM 默认启用 + 加载嵌入式模型；`dict_drop()` 释放
- [x] `dict_cut.c`：`dict_segment_cjk()` 中检测 Trie 未覆盖的 OOV 片段 → 调用 `dict_hmm_viterbi()`
- [x] `dict.h`：新增 `dict_enable_hmm()`/`dict_set_hmm_model()` 声明
- [x] 测试：纯未登录词文本的分词正确性测试；混合文本（登录词 + OOV）测试

## Phase 5：集成验证

- [x] 编译：确保 `dict/` 和 `BM25/` 两个模块编译通过
- [x] BM25 回归测试：运行 BM25 现有测试，确认搜索精度/召回无退化
- [x] 性能对比：记录优化前后 `dict_cut()` 和 `dict_normalize_term()` 的时间开销（基准文本集 + 大词库）
