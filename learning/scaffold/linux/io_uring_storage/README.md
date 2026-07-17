# io_uring_storage — io_uring 高速存储 I/O

## 简介

演示 io_uring 在数据库存储引擎中的应用，包括批量提交、异步读写和存储优化模式。

## 编译

```bash
gcc -std=c11 -Wall -o io_uring_storage main.c
```

## 运行

```bash
./io_uring_storage
```

## 核心概念

### io_uring 存储优势

- **批量提交**：多个 I/O 请求一次系统调用
- **零拷贝**：固定缓冲区减少内存拷贝
- **低延迟**：SQ 轮询模式绕过调度器

### 与 libaio 对比

| 特性 | libaio | io_uring |
|------|--------|----------|
| 系统调用 | 每次 2 次 | 批量 1 次 |
| 内存拷贝 | 1 次 | 0 次 |
| 延迟 | ~20us | ~5us |
| 吞吐 | 中 | 高 |

## 关键参数

- `IORING_SETUP_SQPOLL`：SQ 轮询模式
- `IORING_SETUP_IOPOLL`：I/O 轮询模式（NVMe）
- `IORING_FEAT_NODROP`：不丢弃事件

## 扩展

- 安装 `liburing-dev` 使用用户层库
- 阅读 `man io_uring_enter` 了解详细 API
