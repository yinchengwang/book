# NOTES - 工程对照

## 工程源码对照

`engineering/src/db/storage/page/disk.c` 是我们的 DB 存储引擎页面管理模块，使用 `pread/pwrite` 进行原子性读写。

### 当前实现

```c
// disk.c:72-78（Linux 实现）
static ssize_t file_pread(int fd, void *buf, size_t count, off_t offset) {
    return pread(fd, buf, count, offset);
}

static ssize_t file_pwrite(int fd, const void *buf, size_t count, off_t offset) {
    return pwrite(fd, buf, count, offset);
}

// disk.c:284-286
ssize_t n = file_pread(file->fd, page, file->page_size, offset);
```

当前使用的是传统的 `read/write` 模式（对应零拷贝卡中的 read/write 方式）。

### 零拷贝优化方向

对于 DB 存储引擎，零拷贝主要应用场景：

1. **WAL 日志写入**
   - 写前日志需要频繁写入
   - 可以考虑使用 `write` + `fsync`
   - sendfile 不太适合（需要追加写）

2. **页面刷盘**
   - Buffer Pool 刷脏页到磁盘
   - 当前使用 `pwrite`
   - 页面数据已经在内存中，直接写入

3. **数据复制**
   - 从磁盘读取后复制到网络缓冲区
   - 可以考虑 `mmap` 减少一次拷贝

### 实际优化建议

| 场景 | 当前方案 | 优化方案 |
|------|---------|---------|
| 页面读取 | pread | mmap（减少一次拷贝） |
| 页面写入 | pwrite | pwrite（追加写不适合 sendfile） |
| 数据导出 | read+write | sendfile（整文件传输） |

### 为什么 DB 主要用 pread/pwrite？

1. **原子性**：`pread/pwrite` 在指定偏移读写，不影响文件指针
2. **随机访问**：DB 页面可能在任意位置
3. **并发安全**：多个线程可以同时读写不同页面

sendfile 主要适用于：
- 顺序读取整个文件（如 HTTP 静态文件服务）
- 从文件到 socket 的直接传输

### 性能测量

可以在 DB 中添加 I/O 性能指标：

```c
// 添加性能测量
struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC, &start);

disk_read_page(file, page_id);

clock_gettime(CLOCK_MONOTONIC, &end);
double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                 (end.tv_nsec - start.tv_nsec) / 1000000.0;
// 记录到指标
```

## 工程实践

1. **小文件**：pread/pwrite 的额外偏移计算开销可忽略
2. **大文件**：考虑 mmap 减少拷贝
3. **静态资源服务**：sendfile 是最佳选择
4. **DB 存储**：当前实现已足够高效

## 学习路径

零拷贝是高性能网络编程的重要技术。配合：
- **socket** 卡 → 理解网络 I/O
- **epoll** 卡 → 理解高性能事件驱动
- **本卡** → 理解数据拷贝优化
