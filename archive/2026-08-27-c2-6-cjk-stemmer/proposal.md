# C2-6 Document 中文分词与词干化 Proposal

## Why

差距分析 06 卷发现：Document 模态已有 standard/whitespace/keyword 三 tokenizer（`doc_fts.c:196/285/346`）+ DocSynonyms（`:97-175`），但**缺中文分词与英文词干化**——中文按字符切分（召回率低），英文不分词干变体（runs/jumps/jumped 不归一）——与 Lucene/ES 的 IK/Snowball 生态差距大。用户决定外围全部自研，不集成开源。

## What Changes

- 分析器链式框架：char filter → tokenizer → token filter 三段管线（DocTokenizer 接口扩为链）
- **自研中文词典分词**：
  - 正向最大匹配（FMM）+ 逆向最大匹配（RMM）双向校验
  - 词典 1-2 万词条起步（外部文件加载，可热更新）
  - 未登录词 fallback bigram（二元组）
  - 数据源：现代汉语常用词表 + 通用领域词典
- **自研 Snowball English (Porter2)**：
  - 公开算法移植（~500 行 C），无 license 风险
  - 单语先行（英文），其余语种接口预留
- tokenizer 链示例：`lowercase char filter → standard tokenizer → Porter stemmer token filter`

## Capabilities

| 能力 | 交付 |
|------|------|
| 链式分析器 | 任意 char filter/tokenizer/token filter 组合 |
| 中文分词 | 中文检索 Recall 比字符切分提升（基准数据集） |
| 英文词干 | runs/jumps/jumped 查询统一匹配 |
| 算法可移植 | 公开算法实现，无 license 风险 |

## Impact

- 修改：doc_fts.c、DocTokenizer 接口扩展
- 新增：analyzer_chain.c、cjk_tokenizer.c、snowball_porter2.c、词典文件
- 预计 4-5 个 commit
