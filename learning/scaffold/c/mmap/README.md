# mmap scaffold

`mmap(2)` 演示：把文件直接映射进进程虚拟地址空间，避免 `read` + 缓冲区的二次拷贝。MAP_PRIVATE 触发写时复制（COW）。

## 复现命令

仅 POSIX 平台（WSL/Linux/macOS）：

```bash
cd learning/scaffold/mmap
gcc -Wall -Wextra -O2 -std=c11 -o mmap_demo main.c
make test                       # 自动用 /dev/urandom 生成 4KB 临时文件测试
# 或手动：
head -c 4096 /dev/urandom > /tmp/x
./mmap_demo /tmp/x
```

## 预期输出

```
[mmap] first bytes:
ab cd 1f 23 89 4a ... (前 256 字节 hex)
[mmap] OK: 4096 bytes mapped
```

## 在本机（MinGW-w64）的现状

- `<sys/mman.h>` 不可用（MinGW 故意剥离）
- `#error` 已显式触发
- 在 POSIX 平台直接跑

## 关键点

- **MMAP_PRIVATE + PROT_READ**：内核用 page cache；多次 mmap 同一文件共享物理页
- **madvise(MADV_WILLNEED)**：提前触发 page fault 预读，对大文件首屏访问关键
- **munmap 必须**：否则 munmap 段错误；进程正常退出内核自动回收
- **PROT_WRITE 不能跨 MAP_PRIVATE 回写原文件**：写时复制是"私事"

详见 NOTES.md 工程对照段。
