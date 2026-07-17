# Linux 系统调用学习笔记

本文档记录 Linux 系统调用在项目工程代码中的实际应用对照。

## 工程对照

### 1. open/read/write/close 对照

在项目 `engineering/src/db/storage/page/disk.c` 中，文件操作被封装为跨平台接口：

```c
// disk.c 第 171 行（Linux 平台）
file->fd = open(path, O_RDWR | O_CREAT, 0644);
if (file->fd < 0) {
    free(file->path);
    free(file);
    return NULL;
}
```

**对比学习：**

| 层级 | 学习卡片 | 工程实现 |
|------|----------|----------|
| 创建文件 | `open(O_CREAT)` | 封装在 `disk_open()` 中 |
| 读取页面 | `read()` | 使用 `file_pread()` 封装 |
| 写入页面 | `write()` | 使用 `file_pwrite()` 封装 |
| 关闭文件 | `close()` | 封装在 `disk_close()` 中 |

**封装模式分析：**

工程代码采用跨平台封装（Windows + Linux），关键模式：

```c
// disk.c 第 72-86 行
#ifdef _WIN32
    // Windows: 使用 ReadFile/WriteFile
#else
    // Linux: 直接调用 pread/pwrite
    static ssize_t file_pread(int fd, void *buf, size_t count, off_t offset) {
        return pread(fd, buf, count, offset);
    }
#endif
```

这种封装的优势：
1. **错误处理统一**：返回值统一为 `-1` 表示错误
2. **跨平台兼容**：Windows/Linux 共用同一套接口
3. **原子性保证**：使用 `pread`/`pwrite` 实现无锁读写

### 2. mmap 对照

在项目 `engineering/src/db/storage/vector/vec_page.c` 中，使用 mmap 实现向量化页面的内存映射：

```c
// vec_page.c 第 651 行
pool->mmap_base = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd, 0);
```

**对比学习：**

| 步骤 | 学习卡片 | 工程实现 |
|------|----------|----------|
| 打开文件 | `open()` | `vector_page_enable_mmap()` 内部调用 |
| 扩展大小 | `ftruncate()` | 同上，确保 mmap 区域有效 |
| 执行映射 | `mmap()` | 存储在 `pool->mmap_base` |
| 解除映射 | `munmap()` | `vector_page_disable_mmap()` |
| 关闭文件 | `close()` | 在 mmap 成功后立即 close |

**工程封装的完整流程（vec_page.c 第 593-665 行）：**

```c
int vector_page_enable_mmap(vector_page_pool_t *pool, size_t mmap_size) {
    // 1. 关闭已存在的 mmap
    if (pool->mmap_base != NULL) {
        vector_page_disable_mmap(pool);
    }

    // 2. 打开/创建文件
    int fd = open(file_path, O_RDWR | O_CREAT, 0644);

    // 3. 扩展文件大小
    ftruncate(fd, mmap_size);

    // 4. 执行 mmap
    pool->mmap_base = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
                           MAP_SHARED, fd, 0);

    // 5. 关闭 fd（映射后仍可访问）
    close(fd);

    // 6. 设置标志
    pool->use_mmap = true;
}
```

**关键工程经验：**

1. **fd 在 mmap 后可关闭**：文件描述符关闭不影响已建立的映射
2. **MAP_SHARED vs MAP_PRIVATE**：
   - `MAP_SHARED`：写入同步到文件（工程采用）
   - `MAP_PRIVATE`：写入不反映到文件
3. **对齐要求**：mmap 偏移量通常要求页面对齐（4096 字节）

### 3. fsync/msync 对照

| 操作 | 学习卡片 | 工程实现 |
|------|----------|----------|
| 文件同步 | `fsync(fd)` | `disk_sync()` 使用 `file_sync()` 封装 |
| 内存同步 | `msync()` | 未直接使用，但 WAL 模块会用到 |

工程中 `disk.c` 第 80-82 行的 `fsync` 封装：

```c
static int file_sync(int fd) {
    return fsync(fd);
}
```

### 4. 错误处理模式总结

工程代码中的错误处理模式：

```c
// 标准模式（disk.c）
file->fd = open(path, O_RDWR | O_CREAT, 0644);
if (file->fd < 0) {
    free(file->path);
    free(file);
    return NULL;  // 向上传递错误
}

// 使用 errno（mmap 失败）
if (mapped == MAP_FAILED) {
    perror("[syscall] mmap 失败");
    // 清理资源
}
```

**学习要点：**

1. syscall 失败时返回 `-1`，成功时返回非负值
2. `mmap` 失败返回特殊值 `MAP_FAILED`（不是 `-1`）
3. 资源泄漏防护：失败时必须清理已分配的资源
4. `perror()` 会自动输出 `: ` 分隔符和错误描述

## 常见错误码参考

| errno | 含义 | 常见场景 |
|-------|------|----------|
| `ENOENT` | 文件不存在 | open() 打开不存在的文件 |
| `EACCES` | 权限不足 | 读取无权限文件 |
| `ENOSPC` | 磁盘空间不足 | write() 或 ftruncate() |
| `EINVAL` | 无效参数 | mmap() 参数错误 |
| `EMFILE` | 文件描述符耗尽 | 打开过多文件 |

## 参考源码

- `engineering/src/db/storage/page/disk.c` — 文件 I/O 封装
- `engineering/src/db/storage/vector/vec_page.c` — mmap 使用示例
- `engineering/src/db/storage/buffer/bufmgr.c` — Buffer Pool 管理（使用 calloc/memset 而非 mmap）
