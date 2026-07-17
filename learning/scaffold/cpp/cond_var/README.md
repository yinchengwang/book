# 条件变量（Condition Variable）学习脚手架

## 概述

本模块演示 C++ 条件变量的核心用法，包括：
- 线程间同步与通知
- 生产者-消费者模型
- 虚假唤醒（Spurious Wakeup）避免

## 核心概念

### 条件变量 API

| 操作 | C++ | POSIX |
|------|-----|-------|
| 创建 | `std::condition_variable` | `pthread_cond_init()` |
| 等待 | `cv.wait(lock)` | `pthread_cond_wait()` |
| 单播 | `cv.notify_one()` | `pthread_cond_signal()` |
| 广播 | `cv.notify_all()` | `pthread_cond_broadcast()` |
| 销毁 | 析构函数 | `pthread_cond_destroy()` |

### 正确用法（必须用 while）

```cpp
// 错误：使用 if 可能导致虚假唤醒后条件仍不满足
if (queue.empty()) {
    cv.wait(lock);
}

// 正确：使用 while 确保条件真正满足
while (queue.empty()) {
    cv.wait(lock);
}
```

### 虚假唤醒（Spurious Wakeup）

POSIX 标准允许 `pthread_cond_wait()` 无故返回，即使没有调用 `signal` 或 `broadcast`。因此必须用 while 循环包裹 wait 调用，检查条件是否真正满足。

## 文件说明

| 文件 | 说明 |
|------|------|
| `main.cpp` | 三个演示：基本通知、生产者-消费者、虚假唤醒 |
| `Makefile` | 编译配置 |
| `README.md` | 本文件 |
| `NOTES.md` | 工程代码交叉引用 |

## 编译运行

```bash
make          # 编译
make run      # 编译并运行
make clean    # 清理
```

## 演示输出示例

```
=== 条件变量演示 ===

--- 示例1：基本线程通知 ---
[Waiter] 等待通知...
[Notifier] 发送通知
[Waiter] 收到通知！

--- 示例2：生产者-消费者队列 ---
[Producer] 生产: 1
[Consumer] 消费: 1
[Producer] 生产: 2
...
```
