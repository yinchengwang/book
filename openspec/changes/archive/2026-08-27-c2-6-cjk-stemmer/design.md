# C2-6 Document 中文分词 + 词干化 设计文档

## 设计目标

修复 06 卷识别的 Document 模态缺陷：缺中文分词（按字符切分召回率低）、缺英文词干化（runs/jumps/jumped 不归一）。

## 方案

### 1. 词典格式（T2）

每行一词。`cjk_dict_load(path)` 加载到内存数组 + 长度数组（O(1) 查询）。

### 2. FMM + RMM 分词（T4）

正向最大匹配（Forward Max Match）：从左贪心切最长词（≤6 字符）。
反向最大匹配（Reverse Max Match）：从右贪心。
双向校验：FMM 与 RMM 结果一致时采用；不一致时采用 FMM（粗粒度优先）。
未登录词 fallback bigram（2 字符）。

### 3. Snowball Porter2（T6）

Martin Porter 2002 公开算法移植：
- Step 1a: sses/ies/ss/s
- Step 1b: eed/eedly/ed/ing/ingly → strip + 后续规则
- Step 1c: y → i
- Step 2-5: 主体 5 步（简化：先实现核心，完整 ~200 行后续）

## 不变项

- 链式分析器 API（char filter / tokenizer / token filter）通过 cjk_tokenize + snowball_porter2_stem 串联
- 词典文件可热更新（重新 cjk_dict_load 即可）
- 不引入外部算法库（Ik、Snowball C 库）

## 风险

- 完整 Porter2（Step 2-5）省略，runs/jumped 等部分变体可能未归一
- 词典文件需自备（仓库无现成 1-2 万词现代汉语词表）
