# SurrealDB 学习资源

## 学习目标

- 获取 SurrealDB 的最佳学习资源
- 建立系统化的源码分析路径

## 官方资源

| 资源 | 链接 |
|------|------|
| 官方文档 | https://surrealdb.com/docs |
| GitHub | https://github.com/surrealdb/surrealdb |
| 教程 | https://surrealdb.com/learn |

## 源码分析路径

```
surrealdb/
├── lib/
│   └── surrealdb/
│       ├── sql/           # SurrealQL 解析
│       ├── core/          # 核心引擎
│       ├── dbs/           # 数据库层
│       └── kv/            # KV 存储
```

## 要点总结

- 官方文档和教程是最佳入门资源
- 源码重点在 sql/ 和 core/ 目录

## 思考题

1. SurrealQL 解析器使用什么技术实现？
2. 多存储后端如何抽象？
