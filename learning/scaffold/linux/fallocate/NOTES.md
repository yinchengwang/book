# Linux 文件空间预分配学习笔记

本文档记录 fallocate 空间预分配机制在项目工程代码中的实际应用对照。

## 工程对照

### 1. 数据库文件管理中的空间分配

工程中 `disk.c` 实现了数据库文件的页面管理，其中的空间分配策略与内核 fallocate 形成对照：

```c
// engineering/src/db/storage/page/disk.c:303-356
page_t *disk_alloc_page(db_file_t *file, page_type_t type, page_id_t *out_page_id) {
    // 从空闲链表分配页面，或递增扩展文件
    // ...

    // 扩展文件大小（如果需要）
    off_t file_size = (off_t)file->page_count * file->page_size;
    file_truncate(file->fd, file_size);

    // 更新文件头部并刷盘
    file->header.page_count = file->page_count;
    file_pwrite(file->fd, &file->header, sizeof(file->header), 0);
}
```

**当前实现与 fallocate 的差异：**

| 特性 | disk.c 当前实现 | fallocate 方式 |
|------|----------------|----------------|
| 空间扩展方式 | ftruncate（稀疏扩展） | fallocate（保证物理分配） |
| 磁盘碎片 | 可能产生碎片 | 内核尽量连续分配 |
| 写入失败处理 | 写入时才发现空间不足 | 分配时就知道是否成功 |
| 性能 | 依赖文件系统延迟分配 | 预分配保证顺序性能 |

### 2. 改进建议：使用 fallocate 替代 ftruncate

```c
// 当前方式（ftruncate - 稀疏文件，可能 ENOSPC 延迟发现）
off_t file_size = (off_t)file->page_count * file->page_size;
file_truncate(file->fd, file_size);

// 改进方式（fallocate - 保证物理空间分配）
off_t offset = (off_t)old_page_count * file->page_size;
off_t length = (off_t)(file->page_count - old_page_count) * file->page_size;
#ifdef __linux__
    fallocate(file->fd, 0, offset, length);
#else
    // POSIX 回退：posix_fallocate
    posix_fallocate(file->fd, offset, length);
#endif
```

使用 fallocate 的优势：
1. **原子失败**：磁盘空间不足时立即返回错误，而非延迟到写入时才发现
2. **减少碎片**：内核可一次性分配连续 extent，而非依赖延迟分配
3. **性能可预测**：第一次写入时不需要等待文件系统分配块

### 3. 页面释放与 PUNCH_HOLE 对照

```c
// engineering/src/db/storage/page/disk.c:358-378
int disk_free_page(db_file_t *file, page_id_t page_id) {
    // 将页面加入空闲链表，标记为 PAGE_FREE
    // 注意：当前仅标记空闲，并未释放磁盘空间
    *((page_id_t *)page->data) = file->free_head;
    page->header.page_type = PAGE_FREE;
    disk_write_page(file, page_id, page);
    // ...
}
```

当前实现仅将页面加入空闲链表供复用，并未实际释放磁盘空间。对于大型数据库文件，可以使用 `FALLOC_FL_PUNCH_HOLE` 真正释放不再需要的页面范围，减小磁盘占用：

```c
// 打洞释放不再使用的页面范围
fallocate(file->fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
          page_id * page_size, page_size);
```

### 4. 文件初始化与 KEEP_SIZE

数据库创建时通常希望预分配但不改变逻辑大小（分阶段扩展），这对应 `FALLOC_FL_KEEP_SIZE`：

```c
// 为数据库文件预留 1GB 磁盘空间，但不增加文件大小
// 后续通过 ftruncate 逐步暴露已分配的空间
fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, 1024 * 1024 * 1024);
```

这样可以确保磁盘空间可用，同时保持逻辑大小与实际使用量一致，避免备份工具复制大量零填充区域。

## 参考源码

- `engineering/src/db/storage/page/disk.c` — 数据库文件管理（页面分配/释放/读写）
- `engineering/src/db/storage/page/page.c` — 页面结构定义与创建
