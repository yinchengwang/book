# Redo Log 详解学习卡

## 概述

本卡片演示 Redo Log 的核心概念与崩溃恢复流程：
- Redo Log 格式：LSN/PageID/操作类型/数据
- 检查点机制：记录脏页状态，加速恢复
- 崩溃恢复：从检查点重放 Redo Log

## 编译与运行

```bash
# 编译
gcc -std=c11 -Wall -o redo_log main.c

# 运行
./redo_log
```

## 学习要点

1. **LSN（Log Sequence Number）**：唯一标识每条日志
2. **脏页表**：记录每个脏页的 REC_LSN
3. **检查点**：记录当前脏页状态，恢复时从检查点开始
4. **WAL 原则**：数据修改先写日志，日志落盘才算提交

## 工程对照

见 [NOTES.md](NOTES.md) 中 WAL 实现的工程对照。