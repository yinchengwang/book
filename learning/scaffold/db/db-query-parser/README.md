# SQL 解析器

演示词法分析、语法分析和 AST 构建的核心概念。

## 编译运行

```bash
gcc -std=c11 -Wall -o main main.c && ./main
```

## 演示内容

1. **词法分析**：将 SQL 文本转换为 Token 流
2. **语法分析**：递归下降解析生成 AST
3. **语义分析**：检查类型和引用

## 工程对照

参考 `engineering/src/db/sql/sql_parser.c` 查看完整实现。

## 关联卡片

- db-query-rewrite: 查询重写
- db-logical-plan: 逻辑计划
- db-physical-plan: 物理计划