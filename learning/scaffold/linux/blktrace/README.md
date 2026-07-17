# blktrace — 块设备 I/O 追踪

## 简介

演示 Linux `blktrace` 块设备追踪工具，分析 I/O 请求路径和延迟组成。

## 编译

```bash
gcc -std=c11 -Wall -o blktrace main.c -lpthread
```

## 运行

```bash
./blktrace
```

## 前提条件

```bash
sudo apt install blktrace btrace
```

## 核心功能

### blktrace 架构

```
应用程序 → 通用块层 → I/O 调度器 → 块设备驱动 → 磁盘
              ↑
         blktrace 追踪点
```

### 追踪命令

```bash
# 启动追踪
sudo blktrace -d /dev/sda -o /tmp/trace

# 同时追踪多个操作
blktrace -d /dev/sda -d /dev/sdb -o /tmp/multi

# 设置缓冲区大小
blktrace -b 4096 -d /dev/sda
```

### 分析工具

```bash
# blkparse: 解析追踪数据
blkparse -i /tmp/trace

# 生成二进制格式（可用其他工具分析）
blkparse -i /tmp/trace -d trace.bin

# 实时显示
btrace /dev/sda
```

## I/O 延迟组成

| 阶段 | 说明 | 典型耗时 |
|------|------|----------|
| 队列等待 | 请求在调度队列中 | 0.1~10ms |
| 寻道 | 磁头移动 | 3~10ms |
| 旋转 | 盘片旋转 | 0~5ms |
| 传输 | 数据读写 | 0.01~0.1ms |

## 扩展

- 使用 `iotop` 查看进程 I/O
- 使用 `iostat -x` 查看设备利用率
