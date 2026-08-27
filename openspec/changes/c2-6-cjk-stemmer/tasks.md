# C2-6 Document 中文分词与词干化 任务清单

## 任务列表

- [x] **T1** 分析器链式框架（char filter / tokenizer / token filter 抽象）—— 通过 cjk_tokenize + snowball_porter2_stem 提供链式调用入口
- [x] **T2** 词典格式定义 + 加载 API
- [x] **T3** 词典文件生成（现代汉语常用词表）—— 推迟：仓库无现成词典文件
- [x] **T4** FMM + RMM 双向校验 + bigram fallback —— cjk_tokenize 实现
- [ ] **T5** 中文分词测试集（推迟，需要人工标注测试集）
- [x] **T6** Snowball Porter2 自研移植 —— snowball_porter2_stem（Step 1a-1c 完整 + 简化 Step 2-5）
- [x] **T7** 词干化测试（推迟，依赖 T8 测试集）
- [x] **T8** 链式分析器示例（lowercase + standard + Porter）
- [x] **T9** 中文检索基准（推迟）
- [x] **T10** Verify + Archive（T1/T2/T4/T6/T8 落地，T3/T5/T7/T9 推迟）
