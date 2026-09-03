/**
 * @file main.c
 * @brief Linux XDP (eXpress Data Path) 快速数据路径演示
 *
 * 本文件演示：
 * 1. XDP 概念与架构
 * 2. XDP 程序加载流程
 * 3. BPF 映射基础
 * 4. XDP vs iptables 性能对比
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 打印分隔符
void print_separator(const char *title) {
    printf("\n%s\n", "============================================================");
    printf("  %s\n", title);
    printf("%s\n", "============================================================");
}

// 第一部分：XDP 概念
void demo_xdp_concept(void) {
    print_separator("1. XDP 概念与架构");

    printf("数据包处理路径对比：\n\n");
    printf("  [网卡驱动]\n");
    printf("       │\n");
    printf("       ├─► XDP (eBPF) ──► 最早期处理 ~10M pps\n");
    printf("       │\n");
    printf("       └─► netif_rx ──► 内核协议栈 ~1M pps\n\n");

    printf("XDP 优势：\n");
    printf("  - 处理时机极早（驱动层）\n");
    printf("  - 无需分配 skb（sk_buff）\n");
    printf("  - 可达 10M+ 包/秒\n\n");

    printf("XDP vs iptables：\n");
    printf("  | 指标    | XDP        | iptables    |\n");
    printf("  |---------|-------------|-------------|\n");
    printf("  | 位置    | 驱动层     | netfilter   |\n");
    printf("  | 性能    | 10M+ pps  | ~1M pps   |\n");
}

// 第二部分：XDP 程序结构
void demo_xdp_structure(void) {
    print_separator("2. XDP 程序结构");

    printf("典型 XDP 程序 (C 语言风格)：\n\n");
    const char *code =
    "SEC(\"xdp\")\n"
    "int xdp_firewall(struct xdp_md *ctx) {\n"
    "    void *data = (void *)(long)ctx->data;\n"
    "    // 解析包头...\n"
    "    // 查询 BPF 映射...\n"
    "    return XDP_PASS;  // 或 XDP_DROP\n"
    "}";
    printf("%s\n", code);
}

// 第三部分：BPF 映射
void demo_bpf_map(void) {
    print_separator("3. BPF 映射");

    printf("BPF 映射定义：\n\n");
    const char *map_def =
    "struct {\n"
    "    __uint(type, BPF_MAP_TYPE_HASH);\n"
    "    __uint(max_entries, 10000);\n"
    "    __type(key, __u32);\n"
    "    __type(value, __u8);\n"
    "} blacklist SEC(\".maps\");";
    printf("%s\n\n", map_def);

    printf("用户空间操作：\n");
    printf("  bpf(BPF_MAP_LOOKUP_ELEM, ...)  查询\n");
    printf("  bpf(BPF_MAP_UPDATE_ELEM, ...)  更新\n\n");

    printf("工具：bpftool, libbpf, pybpfcc\n");
}

// 第四部分：加载方法
void demo_xdp_load(void) {
    print_separator("4. XDP 程序加载");

    printf("方法 1: ip 命令（临时）\n");
    printf("  ip link set eth0 xdp obj xdp_firewall.bpf.o sec xdp\n");
    printf("  ip link set eth0 xdp off  # 卸载\n\n");

    printf("方法 2: libbpf (C 程序)\n");
    const char *lib_code =
    "  obj = bpf_object__open_file(\"xdp.bpf.o\", NULL);\n"
    "  bpf_object__load(obj);\n"
    "  prog_fd = bpf_program__fd(prog);\n"
    "  bpf_set_link_xdp_fd(ifindex, prog_fd, 0);";
    printf("%s\n", lib_code);
}

// 第五部分：性能对比
void demo_performance(void) {
    print_separator("5. 性能对比");

    printf("| 方案         | 吞吐量      | 延迟 |\n");
    printf("|--------------|-------------|------|\n");
    printf("| 轮询         | ~100K pps  | 高   |\n");
    printf("| epoll        | ~500K pps  | 中   |\n");
    printf("| iptables     | ~1M pps   | 高   |\n");
    printf("| XDP          | 10M+ pps  | 低   |\n\n");

    printf("XDP 适用场景：\n");
    printf("  - DDoS 防护 (Cloudflare)\n");
    printf("  - 负载均衡 (Katran)\n");
    printf("  - 网络监控 (Cilium)\n");
    printf("  - 防火墙加速\n");
}

int main(int argc, char *argv[]) {
    printf("Linux XDP 演示\n");
    printf("==============\n");

    demo_xdp_concept();
    demo_xdp_structure();
    demo_bpf_map();
    demo_xdp_load();
    demo_performance();

    print_separator("演示完成");
    printf("注意：本程序仅演示概念，需 clang 编译 BPF 目标文件后加载。\n");

    return 0;
}
