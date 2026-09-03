/**
 * @file main.c
 * @brief veth pair 与容器网络演示
 *
 * 演示虚拟以太网对的创建、peer 配置、命名空间关联及网桥连接
 * 纯模拟模式，输出预期行为
 *
 * 编译: gcc -std=c11 -Wall -O2 -o veth_demo main.c
 * 运行: sudo ./veth_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 打印带缩进的命令行 */
static void print_cmd(const char *cmd) {
    printf("    $ %s\n", cmd);
}

/* 打印配置示例 */
static void print_config_example(void) {
    printf("\n");
    printf("========================================\n");
    printf("  veth pair 配置示例\n");
    printf("========================================\n\n");

    printf("--- 1. 创建 veth pair ---\n");
    print_cmd("ip link add veth0 type veth peer name veth1");

    printf("\n--- 2. 查看设备 ---\n");
    print_cmd("ip link show type veth");

    printf("\n--- 3. 配置 IP 并启用 ---\n");
    print_cmd("ip addr add 10.0.0.1/24 dev veth0");
    print_cmd("ip link set veth0 up");
    print_cmd("ip link set veth1 up");

    printf("\n--- 4. 创建网络命名空间 ---\n");
    print_cmd("ip netns add container_ns");

    printf("\n--- 5. 将 veth1 移动到命名空间 ---\n");
    print_cmd("ip link set veth1 netns container_ns");

    printf("\n--- 6. 在命名空间内配置 veth1 ---\n");
    print_cmd("ip netns exec container_ns ip addr add 10.0.0.2/24 dev veth1");
    print_cmd("ip netns exec container_ns ip link set veth1 up");
    print_cmd("ip netns exec container_ns ip link set lo up");

    printf("\n--- 7. 测试连通性 ---\n");
    print_cmd("ip netns exec container_ns ping -c 2 10.0.0.1");
}

/* 演示网桥连接场景 */
static void demo_bridge_connection(void) {
    printf("\n");
    printf("========================================\n");
    printf("  网桥连接示例\n");
    printf("========================================\n\n");

    printf("场景: 将 veth 接入网桥，实现多容器互通\n\n");

    printf("--- 1. 创建网桥 br0 ---\n");
    print_cmd("ip link add br0 type bridge");
    print_cmd("ip link set br0 up");

    printf("\n--- 2. 创建两个 veth pair ---\n");
    print_cmd("ip link add veth-a type veth peer name veth-a-pod");
    print_cmd("ip link add veth-b type veth peer name veth-b-pod");

    printf("\n--- 3. 将 veth-a 和 veth-b 接入网桥 ---\n");
    print_cmd("ip link set veth-a master br0");
    print_cmd("ip link set veth-b master br0");
    print_cmd("ip link set veth-a up");
    print_cmd("ip link set veth-b up");

    printf("\n--- 4. 为网桥分配 IP ---\n");
    print_cmd("ip addr add 172.17.0.1/16 dev br0");

    printf("\n--- 5. 将 veth-a-pod 移入容器并配置 ---\n");
    print_cmd("ip link set veth-a-pod netns pod1");
    print_cmd("ip netns exec pod1 ip addr add 172.17.0.2/16 dev veth-a-pod");
    print_cmd("ip netns exec pod1 ip link set veth-a-pod up");
    print_cmd("ip netns exec pod1 ip route add default via 172.17.0.1");

    printf("\n--- 6. 将 veth-b-pod 移入另一个容器 ---\n");
    print_cmd("ip link set veth-b-pod netns pod2");
    print_cmd("ip netns exec pod2 ip addr add 172.17.0.3/16 dev veth-b-pod");
    print_cmd("ip netns exec pod2 ip link set veth-b-pod up");
    print_cmd("ip netns exec pod2 ip route add default via 172.17.0.1");

    printf("\n--- 7. 容器间通信测试 ---\n");
    print_cmd("ip netns exec pod1 ping -c 1 172.17.0.3");

    printf("\n--- 8. 查看网桥信息 ---\n");
    print_cmd("bridge link show");
    print_cmd("bridge fdb show");
}

/* 演示容器网络拓扑 */
static void demo_container_topology(void) {
    printf("\n");
    printf("========================================\n");
    printf("  Docker 容器网络拓扑\n");
    printf("========================================\n\n");

    printf("典型 Docker bridge 网络:\n\n");
    printf("    +------------------+     +------------------+\n");
    printf("    |   容器 A (eth0)  |     |   容器 B (eth0)  |\n");
    printf("    +--------+---------+     +--------+---------+\n");
    printf("             | vethA                     | vethB\n");
    printf("    +--------+---------+     +--------+---------+\n");
    printf("    |      docker0      |     |                 |\n");
    printf("    |   (bridge)        |     |                 |\n");
    printf("    +--------+---------+     +                 +\n");
    printf("             | veth-host                       |\n");
    printf("    +--------+---------+                       |\n");
    printf("    |      eth0/wlan0    |<- - - - - - - - - - +\n");
    printf("    +-------------------+\n\n");

    printf("Docker 内部操作:\n\n");
    print_cmd("docker network create --driver bridge my-net");
    print_cmd("docker run --network my-net -it alpine sh");
    print_cmd("ip link show");
    print_cmd("brctl show");
}

/* 打印清理命令 */
static void print_cleanup(void) {
    printf("\n");
    printf("========================================\n");
    printf("  清理命令\n");
    printf("========================================\n\n");

    print_cmd("ip link del veth0 2>/dev/null");
    print_cmd("ip link del br0 2>/dev/null");
    print_cmd("ip netns del container_ns 2>/dev/null");
    print_cmd("docker network rm my-net 2>/dev/null");
}

int main(void) {
    printf("========================================\n");
    printf("  veth pair 与容器网络演示\n");
    printf("========================================\n");
    printf("\n本程序演示 Linux veth pair 的创建与配置\n");
    printf("输出预期命令，实际执行请使用 sudo\n\n");

    /* veth pair 基础配置 */
    print_config_example();

    /* 网桥连接场景 */
    demo_bridge_connection();

    /* Docker 容器网络拓扑 */
    demo_container_topology();

    /* 清理命令 */
    print_cleanup();

    printf("\n========================================\n");
    printf("  演示完成\n");
    printf("========================================\n\n");

    printf("关键概念:\n");
    printf("  - veth pair 成对出现，数据在一端进、另一端出\n");
    printf("  - peer 端可分配到不同网络命名空间实现隔离\n");
    printf("  - ip link set vethX master br0 将 veth 接入网桥\n");
    printf("  - Docker 容器网络底层使用 veth pair + bridge\n");

    return 0;
}
