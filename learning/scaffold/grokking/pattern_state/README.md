# 状态模式 (State Pattern)

## 简介

状态模式允许对象在内部状态改变时改变其行为, 使对象看起来像是修改了它的类。

本例使用 **TCP 连接状态机** 作为主演示, 并在文末展示了 **状态表驱动的 FSM** 作为对比。

## 目录结构

```
pattern_state/
├── main.py      # 状态模式演示代码 (TCP + FSM 灯开关)
├── Makefile     # 构建和运行配置
├── README.md    # 本文件
└── NOTES.md     # 学习笔记
```

## 运行方式

```bash
make run
# 或直接运行:
python3 main.py
```

## 示例要点

1. 四种连接状态: CLOSED, LISTENING, ESTABLISHED
2. 状态类自身控制状态转换
3. 用多态替代 if-else 状态分支
4. 状态表驱动 FSM 作为备选方案
