## Why

当前 `algo/dict/` 分词词典实现了基于 Trie + DP 的中文分词，已通过 `dict_t` 接口为 BM25 文本检索提供分词能力。但对比 openGauss BM25 使用的 cppjieba 分词器，存在三项能力差距和一项性能瓶颈：

1. **仅支持最优路径分词**（等价于 jieba `Cut` 模式），缺少 `CutForSearch` 子词召回增强，导致 BM25 对复合词的召回受限
2. **停用词与同义词查询为 O(n) 链表遍历**，每个 token 归一化时都要走两轮线性查找
3. **缺少基于统计的未登录词识别**（HMM Viterbi），依赖硬编码的单字切分后备策略
4. **Trie 顶层节点 uthash 开销大**，首层 Unicode 码点查找在大量词汇时产生不必要的哈希开销

## What Changes

- **新增 CutForSearch 分词模式**：在最优路径 DP 结果基础上，额外输出路径中子词，提高 BM25 搜索召回率
- **停用词/同义词哈希化**：用 FNV-1a 哈希 + 拉链法替代链表，O(1) 查询替换 O(n)
- **新增 HMM 未登录词识别**：Viterbi 算法 + 预置 HMM 模型数据，处理 Trie 中未注册的连续 CJK 字符
- **Trie 顶层数组化**：首层（或前 N 层）用固定大小数组替代 uthash，减少哈希计算和碰撞探测

## Capabilities

### New Capabilities

- `dict-cut-for-search`: 新增 `dict_cut_for_search()` 接口，在最优分词基础上扩展子词 token，保持与 `dict_cut()` 相同的输入输出签名
- `dict-hash-lookup`: 停用词和同义词内部存储从链表改为拉链哈希表，`dict_is_stop_word()` / `dict_resolve_synonym()` 为 O(1) 均摊
- `dict-hmm-oov`: 新增 `dict_enable_hmm()` / `dict_set_hmm_model()` 接口，启用后 `dict_cut()` 和 `dict_cut_for_search()` 自动对未登录 CJK 连续片段执行 Viterbi 解码
- `dict-trie-optimize`: Trie 内部根据 Unicode 码点范围将顶层边存储改为直接索引数组，减少 uthash 开销

### Modified Capabilities

- （无已有 spec 文件，此项留空）

## Impact

- **新增文件**：`src/algo/dict/dict_hmm.c`（HMM Viterbi 解码）、`src/algo/dict/dict_hmm_model.c`（嵌入式 HMM 模型数据）
- **修改文件**：
  - `dict_private.h` — 新增 HMM 结构体、哈希桶结构体、Trie 节点顶层数组、CutForSearch 内部函数声明
  - `dict_core.c` — 停用词/同义词初始化改用哈希表、Trie 节点 create 时顶层用数组
  - `dict_cut.c` — 新增 `dict_cut_for_search()` 入口、切割路径中集成 HMM OOV 处理
  - `dict_load.c` — 可能调整大词库加载时的槽位预分配策略
  - `dict.h` — 新增 `dict_cut_for_search()`、`dict_enable_hmm()`、`dict_set_hmm_model()` 声明
- **依赖**：无新增外部依赖
- **向后兼容**：所有现有 `dict_t` 公开接口签名不变；BM25 无需任何代码修改即可继续使用；新功能通过新增函数暴露，BM25 后续可选择性迁移 `dict_cut` → `dict_cut_for_search`
