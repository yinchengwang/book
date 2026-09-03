# 查询重写

演示查询重写的核心规则：谓词下推、常量折叠、子查询展开。

## 编译运行

```bash
gcc -std=c11 -Wall -o main main.c && ./main
```

## 工程对照

参考 `engineering/src/db/sql/` 目录。