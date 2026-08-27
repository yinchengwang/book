# C2-6 Document 中文分词与词干化 任务清单

## 任务列表

- [ ] **T1** 分析器链式框架（char filter / tokenizer / token filter 抽象）
- [ ] **T2** 词典格式定义 + 加载 API
- [ ] **T3** 词典文件生成（现代汉语常用词表，1-2 万词条）
- [ ] **T4** FMM + RMM 双向校验 + bigram fallback
- [ ] **T5** 中文分词测试集（命中已知答案）
- [ ] **T6** Snowball Porter2 自研移植
- [ ] **T7** 词干化测试（runs/jumps/jumped/ran 归一）
- [ ] **T8** 链式分析器示例（lowercase + standard + Porter）
- [ ] **T9** 中文检索基准（与字符切分对比）
- [ ] **T10** Verify + Archive
