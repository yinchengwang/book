# S19 Spec —— Learn Deep 数据双源契约

## 1. 数据流

```
学习者 (Obsidian)
   ↓ 写 markdown
learning/notes/learn_deep/{c,cpp,db,ds,grok,illustrate}/*.md  (主存储)
   ↓ sync-learn-deep.sh
apps/web/knowledge_hub/src/data/learn_deep/{c,cpp,db,ds,grok,illustrate}/*.md  (副本)
   ↓ 知识库小程序 bundle
H5 + WeApp 双端
```

## 2. 学习层 notes/README.md 要求

含 "Learn Deep 来源" 段：

```
Learn Deep 数据来源
==================

Learn Deep 数据来源：apps/web/knowledge_hub/src/data/learn_deep/
（运行 learning/scripts/sync-learn-deep.sh 同步）
```

## 3. knowledge_hub/CLAUDE.md 要求

含 "数据源" 段：

```
数据源
======

本应用 Learn Deep 内容数据来自：
  ../learning/notes/learn_deep/

使用 learning/scripts/sync-learn-deep.sh 双向同步。
```

## 4. 不做

- ❌ 实际双向 sync 脚本（先 stub echo）
- ❌ Git LFS / 二进制存储
- ❌ 实际迁移现有 markdown 内容
