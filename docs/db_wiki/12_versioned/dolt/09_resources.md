# Dolt 学习资源

## 学习目标

- 获取 Dolt 的最佳学习资源
- 建立源码分析路径

## 官方资源

| 资源 | 链接 |
|------|------|
| 官方文档 | https://docs.dolthub.com |
| GitHub | https://github.com/dolthub/dolt |
| DoltHub | https://www.dolthub.com |

## 源码分析路径

```
dolt/
├── go/
│   ├── stores/     # 存储引擎
│   │   └── prolly/ # Prolly-Tree 实现
│   ├── sql/        # SQL 引擎
│   └── commands/   # CLI 命令
```

## 要点总结

- 官方文档和 DoltHub 是主要学习资源
- 源码重点在 stores/prolly/ 目录

## 思考题

1. Prolly-Tree 的 Go 实现与标准 B-Tree 有何不同？
2. Dolt 的 SQL 引擎基于哪个开源项目？
