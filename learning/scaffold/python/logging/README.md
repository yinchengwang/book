# logging - Python 日志系统

## 概述

本卡演示 Python logging 模块的配置、多 handler 和层级管理。

## 核心概念

| 概念 | 说明 |
|------|------|
| Logger | 日志记录器，核心 API 接口 |
| Handler | 处理器，决定日志输出目标（控制台/文件） |
| Formatter | 格式化器，定义日志输出格式 |
| Level | 日志级别：DEBUG < INFO < WARNING < ERROR < CRITICAL |
| Filter | 过滤器，细粒度控制日志记录 |

## 快速开始

```bash
# 运行演示
make run

# 或直接执行
python3 main.py
```

## 演示内容

1. **基础 logging** - 级别配置、基础 API
2. **多 Handler** - 控制台 + 文件同时输出
3. **JSON 配置** - 字典配置方式
4. **Logger 层级** - 父子 logger 继承关系
5. **旋转日志** - 大小/时间轮转

## 依赖

- Python 3.6+

## 验证

```bash
make test
```

## 常见问题

**Q: 日志不输出怎么办？**
A: 检查 logger 级别和 handler 级别，确保 handler 已添加到 logger。

**Q: 如何同时输出到控制台和文件？**
A: 添加多个 handler 到同一个 logger。

**Q: logging vs print？**
A: 生产环境用 logging，支持级别控制、格式化、文件输出、轮转。
