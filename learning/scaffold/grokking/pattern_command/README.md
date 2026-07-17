# 命令模式

## 描述

命令模式（Command Pattern）将请求封装为对象，从而使你可以用不同的请求对客户端进行参数化、对请求排队或记录请求日志，以及支持可撤销的操作。

## 目录结构

```
pattern_command/
├── main.py      # 主程序：命令模式实现
├── Makefile     # 构建/运行脚本
├── README.md    # 本文件
└── NOTES.md     # 工程对比与面试要点
```

## 运行

```bash
cd learning/scaffold/grokking/pattern_command
make run
```

## 核心角色

| 角色 | 类 | 职责 |
|------|-----|------|
| Command | `Command` | 声明 execute/undo 接口 |
| ConcreteCommand | `InsertCommand` / `DeleteCommand` | 实现具体操作 |
| Receiver | `TextEditor` | 执行业务逻辑 |
| Invoker | `CommandHistory` | 管理命令队列与撤销/重做栈 |
