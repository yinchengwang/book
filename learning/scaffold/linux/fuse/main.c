/**
 * @file main.c
 * @brief Linux FUSE 用户态文件系统学习演示
 *
 * 演示内容：
 *   - FUSE 框架架构（/dev/fuse 设备、libfuse 库、内核模块）
 *   - fuse_operations 结构体与回调函数注册
 *   - 简化内存文件系统的实现（getattr/readdir/read/write/create）
 *   - FUSE 请求生命周期（用户态↔内核态消息传递）
 *
 * 编译：gcc -std=c11 -Wall -o fuse main.c
 * 运行：./fuse
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

/* ============================================================
 * 简化的 FUSE 协议常量（无需 libfuse 依赖）
 * ============================================================ */

#define FUSE_ROOT_INO      1       /* 根目录 inode */
#define MAX_FILES         64       /* 最大文件数 */
#define MAX_NAME_LEN      64       /* 最大文件名长度 */
#define MAX_DATA_SIZE    4096      /* 最大文件数据大小 */

/* 文件类型 */
#define FT_REGULAR        1       /* 普通文件 */
#define FT_DIRECTORY      2       /* 目录 */

/* 文件属性标志 */
#define FUSE_FATTR_MODE   (1 << 0)
#define FUSE_FATTR_SIZE   (1 << 1)
#define FUSE_FATTR_ATIME  (1 << 2)
#define FUSE_FATTR_MTIME  (1 << 3)
#define FUSE_FATTR_CTIME  (1 << 4)

/* ============================================================
 * 内存文件系统中的文件条目
 * ============================================================ */
typedef struct {
    int         ino;                        /* inode 编号 */
    int         type;                       /* FT_REGULAR 或 FT_DIRECTORY */
    char        name[MAX_NAME_LEN];         /* 文件名 */
    mode_t      mode;                       /* 文件权限 */
    size_t      size;                       /* 文件大小 */
    char        data[MAX_DATA_SIZE];        /* 文件数据（内存存储） */
    time_t      atime;                      /* 访问时间 */
    time_t      mtime;                      /* 修改时间 */
    time_t      ctime;                      /* 状态改变时间 */
    int         parent_ino;                 /* 父目录 inode */
    int         nlink;                      /* 硬链接数 */
} memfs_entry_t;

/* ============================================================
 * 内存文件系统全局状态
 * ============================================================ */
static memfs_entry_t files[MAX_FILES];
static int file_count = 0;

/* ============================================================
 * FUSE 操作回调类型定义（模拟 fuse_operations）
 * ============================================================ */

/** 获取文件属性 */
typedef int (*fuse_getattr_t)(int ino, struct stat *stbuf);

/** 读取目录项 */
typedef int (*fuse_readdir_t)(int ino, void *buf,
                               int (*filler)(void *, const char *, int));

/** 读取文件内容 */
typedef int (*fuse_read_t)(int ino, char *buf, size_t size, off_t offset);

/** 写入文件内容 */
typedef int (*fuse_write_t)(int ino, const char *buf, size_t size, off_t offset);

/** 创建文件 */
typedef int (*fuse_create_t)(int parent_ino, const char *name, mode_t mode);

/** fuse_operations 结构体（模拟 libfuse 的 struct fuse_operations） */
typedef struct {
    fuse_getattr_t   getattr;
    fuse_readdir_t   readdir;
    fuse_read_t      read;
    fuse_write_t     write;
    fuse_create_t    create;
} fuse_operations_t;

/* ============================================================
 * 辅助函数：查找文件条目
 * ============================================================ */
static memfs_entry_t *find_entry_by_ino(int ino) {
    for (int i = 0; i < file_count; i++) {
        if (files[i].ino == ino) return &files[i];
    }
    return NULL;
}

static memfs_entry_t *find_entry_by_name(int parent_ino, const char *name) {
    for (int i = 0; i < file_count; i++) {
        if (files[i].parent_ino == parent_ino &&
            strcmp(files[i].name, name) == 0)
            return &files[i];
    }
    return NULL;
}

/* ============================================================
 * FUSE 操作实现：内存文件系统
 * ============================================================ */

static int memfs_getattr(int ino, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (ino == FUSE_ROOT_INO) {
        /* 根目录属性 */
        stbuf->st_mode  = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_size  = 4096;
        return 0;
    }

    memfs_entry_t *entry = find_entry_by_ino(ino);
    if (!entry) return -ENOENT;

    stbuf->st_mode  = entry->mode;
    stbuf->st_nlink = entry->nlink;
    stbuf->st_size  = entry->size;
    stbuf->st_atime = entry->atime;
    stbuf->st_mtime = entry->mtime;
    stbuf->st_ctime = entry->ctime;
    return 0;
}

static int memfs_readdir(int ino, void *buf,
                         int (*filler)(void *, const char *, int)) {
    if (ino != FUSE_ROOT_INO) return -ENOTDIR;

    /* 填充 "." 和 ".." 条目 */
    filler(buf, ".",  FUSE_ROOT_INO);
    filler(buf, "..", FUSE_ROOT_INO);

    /* 填充所有子文件/目录 */
    for (int i = 0; i < file_count; i++) {
        if (files[i].parent_ino == ino) {
            filler(buf, files[i].name, files[i].ino);
        }
    }
    return 0;
}

static int memfs_read(int ino, char *buf, size_t size, off_t offset) {
    memfs_entry_t *entry = find_entry_by_ino(ino);
    if (!entry) return -ENOENT;
    if (entry->type != FT_REGULAR) return -EISDIR;

    if ((size_t)offset >= entry->size) return 0;
    if (offset + (off_t)size > (off_t)entry->size)
        size = entry->size - offset;

    memcpy(buf, entry->data + offset, size);
    entry->atime = time(NULL);
    return (int)size;
}

static int memfs_write(int ino, const char *buf, size_t size, off_t offset) {
    memfs_entry_t *entry = find_entry_by_ino(ino);
    if (!entry) return -ENOENT;
    if (entry->type != FT_REGULAR) return -EISDIR;

    if (offset + (off_t)size > MAX_DATA_SIZE) return -ENOSPC;

    memcpy(entry->data + offset, buf, size);
    if ((size_t)(offset + size) > entry->size)
        entry->size = offset + size;
    entry->mtime = time(NULL);
    return (int)size;
}

static int memfs_create(int parent_ino, const char *name, mode_t mode) {
    if (file_count >= MAX_FILES) return -ENOSPC;
    if (find_entry_by_name(parent_ino, name)) return -EEXIST;

    int idx = file_count++;
    files[idx].ino        = FUSE_ROOT_INO + 1 + idx;
    files[idx].type       = FT_REGULAR;
    files[idx].mode       = S_IFREG | mode;
    files[idx].size       = 0;
    files[idx].parent_ino = parent_ino;
    files[idx].nlink      = 1;
    memset(files[idx].data, 0, MAX_DATA_SIZE);
    time_t now = time(NULL);
    files[idx].atime = now;
    files[idx].mtime = now;
    files[idx].ctime = now;
    strncpy(files[idx].name, name, MAX_NAME_LEN - 1);
    files[idx].name[MAX_NAME_LEN - 1] = '\0';

    return files[idx].ino;
}

/* ============================================================
 * FUSE 操作表注册
 * ============================================================ */
static const fuse_operations_t memfs_ops = {
    .getattr = memfs_getattr,
    .readdir = memfs_readdir,
    .read    = memfs_read,
    .write   = memfs_write,
    .create  = memfs_create,
};

/* ============================================================
 * 模拟 FUSE 请求分发
 * ============================================================ */
static void fuse_dispatch_demo(const fuse_operations_t *ops) {
    struct stat st;

    printf("[fuse] 模拟 FUSE 请求分发:\n");

    /* 请求 1: 获取根目录属性 */
    printf("[fuse]   → LOOKUP / → getattr(%d)\n", FUSE_ROOT_INO);
    if (ops->getattr(FUSE_ROOT_INO, &st) == 0) {
        printf("[fuse]     ← 属性: mode=%o, size=%ld\n",
               st.st_mode, (long)st.st_size);
    }

    /* 请求 2: 创建文件 /hello.txt */
    printf("[fuse]   → MKNOD /hello.txt → create(%d, \"hello.txt\", 0644)\n",
           FUSE_ROOT_INO);
    int new_ino = ops->create(FUSE_ROOT_INO, "hello.txt", 0644);
    if (new_ino > 0) {
        printf("[fuse]     ← 成功, ino=%d\n", new_ino);
    }

    /* 请求 3: 写入数据 */
    const char *msg = "Hello from FUSE 内存文件系统!";
    printf("[fuse]   → WRITE /hello.txt → write(%d, \"%s\")\n", new_ino, msg);
    int written = ops->write(new_ino, msg, strlen(msg), 0);
    printf("[fuse]     ← 写入 %d 字节\n", written);

    /* 请求 4: 读取数据 */
    printf("[fuse]   → READ /hello.txt → read(%d, 128, 0)\n", new_ino);
    char readbuf[128] = {0};
    int nread = ops->read(new_ino, readbuf, sizeof(readbuf) - 1, 0);
    printf("[fuse]     ← 读取 %d 字节: \"%s\"\n", nread, readbuf);

    /* 请求 5: 获取文件属性 */
    printf("[fuse]   → GETATTR /hello.txt → getattr(%d)\n", new_ino);
    if (ops->getattr(new_ino, &st) == 0) {
        printf("[fuse]     ← 属性: mode=%o, size=%ld\n",
               st.st_mode, (long)st.st_size);
    }
}

/* ============================================================
 * Demo 1: FUSE 框架架构概览
 * ============================================================ */
static void demo_fuse_arch(void) {
    printf("[fuse] FUSE 框架架构:\n\n");

    printf("[fuse] 三层架构:\n");
    printf("[fuse]   ┌─────────────────────────────────┐\n");
    printf("[fuse]   │  用户空间: 文件系统守护进程       │\n");
    printf("[fuse]   │  (libfuse + fuse_operations)      │\n");
    printf("[fuse]   ├─────────────────────────────────┤\n");
    printf("[fuse]   │  内核空间: FUSE 内核模块           │\n");
    printf("[fuse]   │  (/dev/fuse 字符设备)             │\n");
    printf("[fuse]   ├─────────────────────────────────┤\n");
    printf("[fuse]   │  VFS 层: 虚拟文件系统切换层       │\n");
    printf("[fuse]   └─────────────────────────────────┘\n\n");

    printf("[fuse] 请求流程:\n");
    printf("[fuse]   1. 应用调用 open()/read()/write()\n");
    printf("[fuse]   2. VFS 将请求路由到 FUSE 内核模块\n");
    printf("[fuse]   3. FUSE 模块将请求放入 /dev/fuse 队列\n");
    printf("[fuse]   4. 用户态守护进程从 /dev/fuse 读取请求\n");
    printf("[fuse]   5. libfuse 调用对应的 fuse_operations 回调\n");
    printf("[fuse]   6. 结果通过 /dev/fuse 返回内核 → VFS → 应用\n");
}

/* ============================================================
 * Demo 2: fuse_operations 结构体
 * ============================================================ */
static void demo_fuse_operations(void) {
    printf("[fuse] fuse_operations 结构体:\n\n");

    printf("[fuse] 核心回调函数（共 30+ 个，此处展示关键项）:\n");
    printf("[fuse]   getattr   — 获取文件/目录属性 (stat)\n");
    printf("[fuse]   readdir   — 读取目录内容 (ls)\n");
    printf("[fuse]   read      — 读取文件数据\n");
    printf("[fuse]   write     — 写入文件数据\n");
    printf("[fuse]   create    — 创建文件\n");
    printf("[fuse]   mkdir     — 创建目录\n");
    printf("[fuse]   unlink    — 删除文件\n");
    printf("[fuse]   rename    — 重命名文件\n");
    printf("[fuse]   open      — 打开文件（可选实现）\n");
    printf("[fuse]   release   — 关闭文件（可选实现）\n");
    printf("[fuse]   fsync     — 刷新数据到持久化存储\n\n");

    printf("[fuse] 实现要点:\n");
    printf("[fuse]   - 只需实现需要的操作，未实现的返回 -ENOSYS\n");
    printf("[fuse]   - 错误码使用负 errno 值（如 -ENOENT）\n");
    printf("[fuse]   - 多线程环境需注意线程安全\n");
}

/* ============================================================
 * Demo 3: 内存文件系统实战
 * ============================================================ */
static void demo_memfs(void) {
    printf("[fuse] 内存文件系统实战演示:\n\n");

    /* 使用模拟的 FUSE 操作表 */
    fuse_dispatch_demo(&memfs_ops);

    printf("\n[fuse] 内存文件系统特点:\n");
    printf("[fuse]   - 数据存储在进程内存中\n");
    printf("[fuse]   - 重启后数据丢失（除非持久化）\n");
    printf("[fuse]   - 适合测试、缓存、临时文件\n");
    printf("[fuse]   - 可通过添加序列化实现持久化\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux FUSE 用户态文件系统学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: FUSE 框架架构概览 ---\n");
    demo_fuse_arch();
    printf("\n");

    printf("--- Demo 2: fuse_operations 结构体 ---\n");
    demo_fuse_operations();
    printf("\n");

    printf("--- Demo 3: 内存文件系统实战 ---\n");
    demo_memfs();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
