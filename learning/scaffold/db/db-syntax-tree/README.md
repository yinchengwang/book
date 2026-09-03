# 语法树

演示 AST 的节点结构、遍历方式和转换操作。

## 编译运行

```bash
gcc -std=c11 -Wall -o main main.c && ./main
```

## 工程对照

参考 `engineering/include/db/sql/sql_parser.h` 中的 AST 节点定义。