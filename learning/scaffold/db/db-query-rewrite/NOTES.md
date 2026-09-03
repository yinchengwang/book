# NOTES.md - 查询重写工程对照

## 工程源码位置

`engineering/src/db/sql/` (待创建 rewrite.c)

## 核心重写规则

### 1. 谓词下推 (Predicate Pushdown)

将 WHERE 条件下推到 JOIN 之前执行：

```
原始: SELECT * FROM A JOIN B ON A.id=B.id WHERE B.type='vip'
重写: SELECT * FROM A JOIN (SELECT * FROM B WHERE type='vip') AS B ON A.id=B.id
```

### 2. 常量折叠 (Constant Folding)

在编译时计算常量表达式：

```
原始: WHERE x = 1 + 2
重写: WHERE x = 3
```

### 3. 子查询展开 (Subquery Flattening)

将 IN/EXISTS 子查询转换为 JOIN：

```
原始: WHERE id IN (SELECT id FROM t2)
重写: SELECT * FROM t WHERE EXISTS (SELECT 1 FROM t2 WHERE t.id=t2.id)
```

### 4. 视图合并 (View Merging)

展开嵌套视图定义。

### 5. 冗余消除

- 消除恒真/恒假条件
- 消除重复 JOIN
- 消除无用列

## 面试高频题

1. 谓词下推的原理和作用
2. 如何处理相关子查询
3. 常量折叠的实现方法
4. 子查询展开的边界条件