/*
 * mmap scaffold — mmap 读大文件 + 写时复制 (MAP_PRIVATE) 演示
 *
 * 用法:
 *   ./mmap_demo <file>      # mmap 指定文件到内存并打印前 256 byte
 *
 * 演示要点：
 *   - 用 mmap(2) 把文件直接映射进虚拟地址空间，避免 read+buffer
 *   - MAP_PRIVATE 触发写时复制（COW）：修改不会回写到原文件
 *   - munmap(2) 释放映射
 *
 * 大文件零拷贝场景：
 *   - minivecdb 加载嵌入向量（GB 级）可零拷贝使用，避免 malloc + read 重复
 *   - raft snapshot 落盘时 mmap 后 write 直接走 page cache
 */

/* POSIX-only: 依赖 <sys/mman.h> */
#ifdef _WIN32
# error "mmap scaffold 不支持 Windows；请用 WSL 或原生 Linux/macOS"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

static int mmap_demo(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    struct stat st;
    if (fstat(fd, &st) != 0) { perror("fstat"); close(fd); return 1; }
    if (st.st_size == 0) { fprintf(stderr, "empty file\n"); close(fd); return 1; }

    size_t len = (size_t)st.st_size;
    void *p = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (p == MAP_FAILED) { perror("mmap"); close(fd); return 1; }

    /* 建议内核预读：populate 让 page fault 触发预加载 */
    if (madvise(p, len, MADV_WILLNEED) != 0 && errno != EINVAL && errno != ENOSYS) {
        perror("madvise");
    }

    /* 打印前 256 字节（人读用 hex 列出） */
    size_t show = len < 256 ? len : 256;
    const unsigned char *buf = (const unsigned char *)p;
    fputs("[mmap] first bytes:\n", stdout);
    for (size_t i = 0; i < show; i++) {
        printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) putchar('\n');
    }
    if (show % 16 != 0) putchar('\n');

    /* 验证 COW：尝试写 MAP_PRIVATE 应触发 SIGSEGV（PROT_READ 阻止）。本 demo
       只读不动笔，因此保持 PROT_READ。COW 测试代码段注释保留给读者扩展：
       ptr[0] = 'x';    // <-- 这一行若去掉注释会触发 SIGSEGV
    */

    if (munmap(p, len) != 0) perror("munmap");
    close(fd);
    printf("[mmap] OK: %zu bytes mapped\n", len);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 1;
    }
    return mmap_demo(argv[1]);
}
