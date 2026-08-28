# 阅读顺序

```
1. main.c           全局初始化、连接打开、sqlite3_open / sqlite3_prepare 入口
2. sqlite3.h.in     公共 API 定义，搞懂核心类型和对象模型
3. tokenize.c       SQL 文本 → token
4. parse.y          SQL token → AST（Lemon 语法规则）
5. build.c          表/索引/视图等 schema 构建
6. expr.c           表达式处理（最复杂之一，可以先扫结构）
7. select.c         SELECT 查询的生成逻辑
8. vdbe.c           VDBE 虚拟机执行引擎（核心，值得重点读）
9. btree.c          存储层 B-Tree（表/索引页管理）
10. pager.c         页面缓存 + 事务提交/回滚
11. wal.c           预写日志，崩溃恢复
12. os_unix.c / os_win.c   VFS 文件系统抽象，最后看
```

# 链路

```
SQL 文本 → token → AST → 字节码(VDBE opcode) → B-Tree 读写 → Pager 页面管理 → 文件写回
```

# 官方文档

SQLite Architecture 有权威分层图和说明:https://sqlite.org/arch.html

How SQLite Works：https://sqlite.org/howitworks.html
