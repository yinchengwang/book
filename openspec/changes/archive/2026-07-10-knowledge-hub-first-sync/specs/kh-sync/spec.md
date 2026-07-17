# S27 Spec —— Knowledge_Hub First Sync Round

## 1. 文件契约

6 个学习层示例 .md：

```
learning/notes/learn_deep/
├── c/README.md
├── cpp/README.md
├── db/README.md
├── ds/README.md
├── grok/README.md
└── illustrate/README.md
```

每个 README.md 含简单技术栈说明。

## 2. 同步契约

`bash learning/scripts/sync-learn-deep.sh` 把上述文件复制到：

```
apps/web/knowledge_hub/src/data/learn_deep/
├── c/README.md
├── cpp/README.md
├── db/README.md
├── ds/README.md
├── grok/README.md
└── illustrate/README.md
```

## 3. 不做

- ❌ 双向冲突解决
- ❌ 真实数据迁移
- ❌ CI 自动跑
