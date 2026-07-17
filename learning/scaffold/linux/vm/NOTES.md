# Linux 虚拟内存学习笔记

本文档记录虚拟内存在项目工程代码中的实际应用对照。

## 工程对照

### 1. mmap 在数据库中的应用

工程代码使用 mmap 实现高效的文件 I/O：

```c
// engineering/src/db/storage/vector/vec_page.c
// 使用 mmap 映射向量数据文件

void *mmap_base = mmap(NULL, mmap_size,
                       PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd, 0);

if (mmap_base == MAP_FAILED) {
    return -1;
}
```

**mmap vs read/write：**

| 方式 | 优点 | 缺点 |
|------|------|------|
| read/write | 简单、可控 | 数据复制开销 |
| mmap | 无复制、随机访问 | 占用虚拟地址空间 |

### 2. Buffer Pool 与虚拟内存

工程代码中 Buffer Pool 使用虚拟内存管理：

```c
// engineering/src/db/storage/buffer/bufmgr.c
// Buffer Pool 使用 calloc 分配（虚拟内存+物理内存）

buf_block_t *blocks = (buf_block_t *)calloc(
    pool->max_size, sizeof(buf_block_t)
);

// calloc 分配虚拟内存并立即映射物理内存
// 首次访问不会触发缺页
```

### 3. 内存对齐与页面大小

```c
// 工程代码注意页面对齐
#define PAGE_SIZE 4096

/* 按页对齐分配 */
void *alloc_aligned(size_t size) {
    void *ptr;
    posix_memalign(&ptr, PAGE_SIZE, size);
    return ptr;
}

/* 大页支持 */
#ifdef USE_HUGEPAGE
    void *huge_pages = mmap(NULL, size,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                            -1, 0);
#endif
```

### 4. 内存保护在安全中的应用

工程代码使用 mprotect 实现安全特性：

```c
// 代码段只读保护
mprotect(code_segment, code_size, PROT_READ | PROT_EXEC);

// 数据段可写
mprotect(data_segment, data_size, PROT_READ | PROT_WRITE);

// 栈保护（不可执行）
mprotect(stack_segment, stack_size, PROT_READ | PROT_WRITE);
```

### 5. madvise 在性能优化中的应用

```c
// engineering/src/db/storage/page/page_scan.c
// 大规模顺序扫描时使用 MADV_SEQUENTIAL

madvise(buffer, buffer_size, MADV_SEQUENTIAL);

// 扫描完成后释放缓存
madvise(buffer, buffer_size, MADV_DONTNEED);
```

### 6. 缺页中断与性能

```c
// 查看缺页统计
// cat /proc/vmstat | grep -i pg

// pgfault    - 总缺页数（包括 COW 等）
// pgmajfault - 主缺页数（需要磁盘 I/O）
```

**缺页对性能的影响：**

| 类型 | 耗时 | 触发场景 |
|------|------|----------|
| Minor Fault | < 1ms | COW 页面、共享内存 |
| Major Fault | 5-50ms | 文件读取、换入 |

## 参考源码

- `engineering/src/db/storage/buffer/bufmgr.c` — Buffer Pool 实现
- `engineering/src/db/storage/vector/vec_page.c` — mmap 使用
- `engineering/src/db/storage/page/disk.c` — 页面 I/O
