# 备份恢复 (Backup & Recovery)

## 简介

Grokking 数据库内核——备份恢复篇。演示冷备/热备/增量/恢复。

## 目录结构

```
backup_recovery/
├── main.py    # 演示代码
├── Makefile   # 构建配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
make run
```

## 涵盖内容

- 冷备（停服备份）
- 热备（运行中 + WAL 日志）
- 全量 vs 增量备份
- 恢复演练（备份 → 损坏 → 恢复）
