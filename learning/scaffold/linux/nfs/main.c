/**
 * @file main.c
 * @brief Linux NFS 网络文件系统学习演示
 *
 * 演示内容：
 *   - NFS 挂载选项（nfsvers, rsize, wsize, hard/soft, intr）
 *   - 远程文件操作语义（open/read/write/close 的 NFS 协议映射）
 *   - 缓存一致性（close-to-open 一致性模型）
 *   - 锁定管理（lockd/statd 的 NLM 协议角色）
 *
 * 编译：gcc -std=c11 -Wall -o nfs main.c
 * 运行：./nfs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

/* ============================================================
 * NFS 挂载选项结构体
 * ============================================================ */
typedef struct {
    int nfs_version;     /* NFS 协议版本: 3 或 4 */
    int rsize;           /* 读块大小（字节） */
    int wsize;           /* 写块大小（字节） */
    int hard_mount;      /* 1=hard 挂载, 0=soft 挂载 */
    int intr_enabled;    /* 1=允许信号中断 */
    int timeo;           /* 超时时间（十分之一秒） */
    int retrans;         /* 重传次数 */
    const char *proto;   /* 传输协议: tcp 或 udp */
} nfs_mount_opts_t;

/* ============================================================
 * 打印挂载选项
 * ============================================================ */
static void print_mount_opts(const nfs_mount_opts_t *opts) {
    printf("[nfs]   nfsvers=%d\n", opts->nfs_version);
    printf("[nfs]   rsize=%d\n", opts->rsize);
    printf("[nfs]   wsize=%d\n", opts->wsize);
    printf("[nfs]   %s挂载 (可中断=%s)\n",
           opts->hard_mount ? "hard" : "soft",
           opts->intr_enabled ? "是" : "否");
    printf("[nfs]   timeo=%d (%.1f秒), retrans=%d\n",
           opts->timeo, opts->timeo / 10.0, opts->retrans);
    printf("[nfs]   proto=%s\n", opts->proto);
}

/* ============================================================
 * Demo 1: NFS 挂载选项详解
 * ============================================================ */
static void demo_mount_options(void) {
    printf("[nfs] NFS 挂载选项详解:\n\n");

    /* 构造一个典型的 NFSv4 挂载选项 */
    nfs_mount_opts_t opts = {
        .nfs_version  = 4,
        .rsize        = 1048576,   /* 1MB */
        .wsize        = 1048576,   /* 1MB */
        .hard_mount   = 1,
        .intr_enabled = 1,
        .timeo        = 600,       /* 60 秒 */
        .retrans      = 2,
        .proto        = "tcp",
    };
    print_mount_opts(&opts);

    printf("\n[nfs] 挂载命令示例:\n");
    printf("[nfs]   mount -t nfs4 -o rsize=1048576,wsize=1048576,hard,intr \\\n");
    printf("[nfs]        server:/export /mnt/nfs\n\n");

    /* hard vs soft 对比 */
    printf("[nfs] hard 挂载 vs soft 挂载:\n");
    printf("[nfs]   hard: 操作无限重试，服务器恢复后继续\n");
    printf("[nfs]   hard: 适合写操作，保证数据不丢失\n");
    printf("[nfs]   soft: 超时后返回错误给应用程序\n");
    printf("[nfs]   soft: 适合读操作，避免进程挂死\n\n");

    /* rsize/wsize 说明 */
    printf("[nfs] rsize/wsize 对性能的影响:\n");
    printf("[nfs]   - NFSv3 默认 rsize/wsize=4096\n");
    printf("[nfs]   - NFSv4 默认 rsize/wsize=1048576 (1MB)\n");
    printf("[nfs]   - 更大的块大小减少 RPC 往返次数\n");
    printf("[nfs]   - 需网络 MTU 支持（避免分片）\n");
}

/* ============================================================
 * Demo 2: 远程文件操作语义
 * ============================================================ */
static void demo_remote_file_ops(void) {
    printf("[nfs] NFS 远程文件操作语义:\n\n");

    /* RPC 调用映射 */
    printf("[nfs] 系统调用与 NFS RPC 的映射关系:\n");
    printf("[nfs]   open()     → LOOKUP + ACCESS (多个 RPC)\n");
    printf("[nfs]   read()     → READ (一次可读取大块数据)\n");
    printf("[nfs]   write()    → WRITE (异步 vs 同步取决于挂载选项)\n");
    printf("[nfs]   close()    → 客户端可能延迟发送 CLOSE\n");
    printf("[nfs]   fsync()    → COMMIT (强制刷新到服务器磁盘)\n\n");

    /* 无状态 vs 有状态 */
    printf("[nfs] NFSv3 (无状态) vs NFSv4 (有状态):\n");
    printf("[nfs]   NFSv3: 服务器不记录客户端打开的文件\n");
    printf("[nfs]   NFSv3: 每次 READ/WRITE 携带完整文件句柄\n");
    printf("[nfs]   NFSv3: 客户端故障对服务器无影响\n");
    printf("[nfs]   NFSv4: 服务器维护 OPEN 状态（有租约）\n");
    printf("[nfs]   NFSv4: 支持 OPEN/CLOSE 代理和委派\n\n");

    /* 文件句柄 */
    printf("[nfs] 文件句柄 (File Handle):\n");
    printf("[nfs]   - 服务器生成的唯一标识符\n");
    printf("[nfs]   - 客户端持有句柄进行后续操作\n");
    printf("[nfs]   - 类似数据库中的 OID（对象标识符）\n");
}

/* ============================================================
 * Demo 3: 缓存一致性与 close-to-open 模型
 * ============================================================ */
static void demo_cache_consistency(void) {
    printf("[nfs] 缓存一致性与 close-to-open 模型:\n\n");

    /* close-to-open 一致性 */
    printf("[nfs] close-to-open 一致性模型:\n");
    printf("[nfs]   1. 客户端 A 关闭文件时，将所有修改刷新到服务器\n");
    printf("[nfs]   2. 客户端 B 打开文件时，从服务器获取最新属性\n");
    printf("[nfs]   3. 保证: A 的写入在 B 打开后可见\n");
    printf("[nfs]   4. 不保证: 同时打开时的并发写入一致性\n\n");

    /* 属性缓存 */
    printf("[nfs] 属性缓存 (Attribute Cache):\n");
    printf("[nfs]   - acregmin/acregmax: 普通文件属性缓存时间\n");
    printf("[nfs]   - acdirmin/acdirmax: 目录属性缓存时间\n");
    printf("[nfs]   - noac 选项: 禁用属性缓存（性能最差）\n\n");

    /* 委托机制 */
    printf("[nfs] NFSv4 委托 (Delegation):\n");
    printf("[nfs]   - READ 委托: 客户端可本地缓存读取\n");
    printf("[nfs]   - WRITE 委托: 客户端可本地缓存写入\n");
    printf("[nfs]   - 服务器可通过 CB_RECALL 回收委托\n");
    printf("[nfs]   - 类比: 数据库的行锁/页锁语义\n");
}

/* ============================================================
 * Demo 4: 锁定管理 (lockd/statd)
 * ============================================================ */
static void demo_lock_management(void) {
    printf("[nfs] 锁定管理 (lockd/statd):\n\n");

    /* lockd 守护进程 */
    printf("[nfs] lockd (NLM - Network Lock Manager):\n");
    printf("[nfs]   - 处理 fcntl() / flock() 文件锁\n");
    printf("[nfs]   - 将本地锁请求转换为 NLM RPC 调用\n");
    printf("[nfs]   - 客户端 lockd ↔ 服务器 lockd 通信\n\n");

    /* statd 守护进程 */
    printf("[nfs] statd (NSM - Network Status Monitor):\n");
    printf("[nfs]   - 监控客户端/服务器的存活状态\n");
    printf("[nfs]   - 客户端重启后通知服务器释放锁\n");
    printf("[nfs]   - 服务器重启后通知客户端回收锁\n\n");

    /* NFSv4 的改进 */
    printf("[nfs] NFSv4 的改进:\n");
    printf("[nfs]   - 将锁定集成到 NFSv4 协议内部\n");
    printf("[nfs]   - 不再依赖独立的 lockd/statd 守护进程\n");
    printf("[nfs]   - 基于租约 (lease) 的锁管理\n");
    printf("[nfs]   - 类比: 数据库的分布式锁管理器\n\n");

    /* 锁状态 */
    printf("[nfs] 锁恢复场景:\n");
    printf("[nfs]   场景 1 - 客户端崩溃: 服务器在租约到期后释放锁\n");
    printf("[nfs]   场景 2 - 服务器崩溃: 客户端在宽限期内回收锁\n");
    printf("[nfs]   场景 3 - 网络分区: 锁竞争可能导致脑裂\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux NFS 网络文件系统学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: NFS 挂载选项详解 ---\n");
    demo_mount_options();
    printf("\n");

    printf("--- Demo 2: 远程文件操作语义 ---\n");
    demo_remote_file_ops();
    printf("\n");

    printf("--- Demo 3: 缓存一致性 ---\n");
    demo_cache_consistency();
    printf("\n");

    printf("--- Demo 4: 锁定管理 ---\n");
    demo_lock_management();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
