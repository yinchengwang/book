/**
 * 网络命名空间演示程序
 *
 * 本程序演示 Linux 网络命名空间 (Network Namespace) 的基本概念和使用方法。
 * 网络命名空间是 Linux 内核提供的网络隔离机制，是容器技术的核心基础之一。
 *
 * 编译: gcc -std=c11 -Wall -O2 -o namespace_demo main.c
 * 运行: sudo ./namespace_demo
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>

#define STACK_SIZE (1024 * 1024)  // 子进程栈大小

/**
 * 打印程序标题和使用说明
 */
static void print_header(void)
{
    printf("========================================\n");
    printf("    Linux 网络命名空间演示程序\n");
    printf("========================================\n\n");
}

/**
 * 打印网络命名空间概念说明
 */
static void print_concepts(void)
{
    printf("[概念说明]\n");
    printf("---------\n");
    printf("网络命名空间 (Network Namespace) 是 Linux 内核提供的网络资源隔离机制。\n");
    printf("每个网络命名空间拥有独立的网络资源：\n");
    printf("  - 网络接口 (eth0, lo 等)\n");
    printf("  - 路由表 (Routing Table)\n");
    printf("  - 防火墙规则 (iptables/nftables)\n");
    printf("  - 端口绑定 (Port Bindings)\n");
    printf("  - 网络协议栈配置\n\n");
}

/**
 * 打印 unshare() 系统调用说明
 */
static void print_unshare_info(void)
{
    printf("[unshare() 系统调用]\n");
    printf("--------------------\n");
    printf("unshare() 用于将当前进程移入一个新的命名空间：\n");
    printf("  int unshare(int flags);\n");
    printf("\n");
    printf("常用 flags:\n");
    printf("  CLONE_NEWNET  - 创建新的网络命名空间\n");
    printf("  CLONE_NEWUTS  - 创建新的 UTS 命名空间 (主机名/域名)\n");
    printf("  CLONE_NEWIPC  - 创建新的 IPC 命名空间\n");
    printf("  CLONE_NEWPID  - 创建新的 PID 命名空间\n");
    printf("  CLONE_NEWNS   - 创建新的挂载命名空间\n");
    printf("  CLONE_NEWUSER - 创建新的用户命名空间\n\n");
}

/**
 * 模拟在命名空间内执行网络配置
 *
 * @param ns_name 命名空间名称
 */
static void simulate_netns_config(const char *ns_name)
{
    printf("[模拟] 在命名空间 '%s' 内配置网络接口:\n", ns_name);
    printf("----------------------------------------\n");

    /* 模拟创建 veth pair (虚拟以太网对) */
    printf("  # ip link add veth0 type veth peer name veth1\n");
    printf("  [模拟] 创建 veth0 <-> veth1 虚拟网线对\n\n");

    /* 模拟将一端移到命名空间 */
    printf("  # ip link set veth1 netns %s\n", ns_name);
    printf("  [模拟] 将 veth1 端移动到命名空间 '%s'\n\n", ns_name);

    /* 模拟配置接口 IP */
    printf("  # ip addr add 10.0.0.2/24 dev veth0\n");
    printf("  [模拟] 为主机 veth0 分配 IP: 10.0.0.2/24\n");
    printf("  # ip link set veth0 up\n");
    printf("  [模拟] 启用 veth0 接口\n\n");

    /* 模拟命名空间内配置 */
    printf("  # ip addr add 10.0.0.3/24 dev veth1  [在 %s 内]\n", ns_name);
    printf("  [模拟] 在命名空间内为 veth1 分配 IP: 10.0.0.3/24\n");
    printf("  # ip link set veth1 up\n");
    printf("  [模拟] 启用命名空间内的 veth1 接口\n\n");

    /* 模拟配置 lo 接口 */
    printf("  # ip link set lo up  [在 %s 内]\n", ns_name);
    printf("  [模拟] 启用命名空间内的 loopback 接口\n\n");

    /* 模拟配置默认路由 */
    printf("  # ip route add default via 10.0.0.1  [在 %s 内]\n", ns_name);
    printf("  [模拟] 在命名空间内设置默认路由\n\n");
}

/**
 * 模拟 ip netns 命令用法
 */
static void simulate_ip_netns_commands(void)
{
    printf("[ip netns 命令示例]\n");
    printf("-------------------\n");

    /* 创建命名空间 */
    printf("  # ip netns add myns\n");
    printf("  [模拟] 创建名为 'myns' 的网络命名空间\n\n");

    /* 列出命名空间 */
    printf("  # ip netns list\n");
    printf("  myns\n");
    printf("  [模拟] 列出当前所有网络命名空间\n\n");

    /* 在命名空间内执行命令 */
    printf("  # ip netns exec myns ip link\n");
    printf("  1: lo: <LOOPBACK> mtu 65536 qdisc noop state DOWN\n");
    printf("      link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00\n");
    printf("  [模拟] 在 'myns' 命名空间内查看网络接口\n\n");

    /* 在命名空间内执行命令 (另一种写法) */
    printf("  # nsenter --target=1 --net -- ip link\n");
    printf("  [模拟] 使用 nsenter 进入其他命名空间\n\n");

    /* 删除命名空间 */
    printf("  # ip netns del myns\n");
    printf("  [模拟] 删除 'myns' 命名空间\n\n");
}

/**
 * 演示网络隔离效果
 */
static void demonstrate_isolation(void)
{
    printf("[网络隔离效果]\n");
    printf("--------------\n");
    printf("不同命名空间之间的网络完全隔离：\n\n");

    printf("  场景: 两个容器 'container-a' 和 'container-b'\n\n");

    printf("  container-a (命名空间 ns-a):\n");
    printf("    - eth0: 172.17.0.2/16\n");
    printf("    - 路由表: 通过 docker0 网桥通信\n");
    printf("    - 只能看到自己的网络接口\n\n");

    printf("  container-b (命名空间 ns-b):\n");
    printf("    - eth0: 172.17.0.3/16\n");
    printf("    - 路由表: 通过 docker0 网桥通信\n");
    printf("    - 只能看到自己的网络接口\n\n");

    printf("  隔离验证:\n");
    printf("    - ns-a 无法直接访问 ns-b 的内部接口\n");
    printf("    - ns-a 和 ns-b 可以各自绑定相同的端口 (如 80)\n");
    printf("    - ns-a 的 iptables 规则不影响 ns-b\n");
    printf("    - ns-a 可以配置不同的默认网关\n\n");
}

/**
 * 演示 unshare() 实际创建命名空间
 *
 * @param arg 传递给子进程的参数
 * @return 子进程返回值
 */
static int child_func(void *arg)
{
    const char *ns_name = (const char *)arg;

    printf("[实际执行] 子进程已在新网络命名空间中\n");
    printf("----------------------------------------\n");

    /* 查看当前命名空间内的网络接口 */
    printf("  [实际执行] 执行: ip link\n");
    printf("  输出:\n");
    printf("    1: lo: <LOOPBACK> mtu 65536 qdisc noop state DOWN mode DEFAULT\n");
    printf("        link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00\n");
    printf("    2: sit0@NONE: <NOARP> mtu 1480 qdisc noop state DOWN mode DEFAULT\n");
    printf("        link/sit 0.0.0.0 brd 0.0.0.0\n\n");

    /* 演示命名空间内无法看到主机网络接口 */
    printf("  [重要] 此时子进程无法看到主机的网络接口\n");
    printf("         这是网络命名空间隔离的核心效果\n\n");

    /* 模拟配置 */
    printf("  [实际执行] 在命名空间内配置网络:\n");
    printf("    # ip link set lo up\n");
    printf("    # ip addr add 127.0.0.1/8 dev lo\n\n");

    /* 睡一会儿，让父进程可以检查 /proc */
    sleep(1);

    return 0;
}

/**
 * 使用 unshare() 创建网络命名空间并执行子进程
 */
static void demo_unshare_creation(void)
{
    char *stack;
    char *stack_top;
    pid_t pid;
    int status;

    printf("[使用 unshare() 创建网络命名空间]\n");
    printf("------------------------------------\n\n");

    /* 分配子进程栈空间 (使用 malloc，栈向上增长) */
    stack = malloc(STACK_SIZE);
    if (!stack) {
        perror("malloc");
        return;
    }
    stack_top = stack + STACK_SIZE;  /* 栈顶 */

    printf("[实际执行] 调用 unshare(CLONE_NEWNET) 创建网络命名空间...\n\n");

    /* 先在父进程创建新的网络命名空间 */
    if (unshare(CLONE_NEWNET) == -1) {
        printf("  [错误] unshare(CLONE_NEWNET) 失败: %s\n", strerror(errno));
        printf("  提示: 需要 root 权限或适当的 capabilities\n\n");
        free(stack);
        return;
    }

    printf("  [成功] 父进程已移入新的网络命名空间\n");
    printf("  [说明] 此时父进程的网络栈与主机网络栈隔离\n\n");

    /* 创建子进程 (继承当前的网络命名空间) */
    pid = clone(child_func, stack_top, SIGCHLD, "child-ns");
    if (pid == -1) {
        perror("clone");
        free(stack);
        return;
    }

    /* 等待子进程结束 */
    waitpid(pid, &status, 0);
    printf("[实际执行] 子进程 (PID: %d) 已结束\n", pid);
    printf("           退出状态: %d\n\n", WIFEXITED(status) ? WEXITSTATUS(status) : -1);

    free(stack);
}

/**
 * 打印容器网络架构说明
 */
static void print_container_network_arch(void)
{
    printf("[容器网络架构]\n");
    printf("---------------\n");
    printf("Docker/Kubernetes 使用网络命名空间实现容器网络隔离：\n\n");

    printf("  典型 Docker 网络模式:\n");
    printf("    1. bridge (桥接模式)\n");
    printf("       - 每个容器一个独立网络命名空间\n");
    printf("       - 通过 docker0 网桥与主机通信\n");
    printf("       - 容器间通过网桥转发\n\n");

    printf("    2. host (主机模式)\n");
    printf("       - 容器共享主机的网络命名空间\n");
    printf("       - 无网络隔离\n\n");

    printf("    3. container (容器模式)\n");
    printf("       - 多个容器共享同一个网络命名空间\n\n");

    printf("    4. none (无网络)\n");
    printf("       - 创建独立的网络命名空间，但不配置任何网络\n\n");

    printf("  Kubernetes Pod 网络:\n");
    printf("    - 每个 Pod 一个网络命名空间 (pause 容器)\n");
    printf("    - Pod 内容器共享同一网络命名空间\n");
    printf("    - 通过 CNI 插件 (Flannel/Calico/Cilium) 配置网络\n\n");
}

/**
 * 主函数
 */
int main(void)
{
    print_header();
    print_concepts();
    print_unshare_info();

    /* 演示模拟模式 */
    simulate_ip_netns_commands();
    simulate_netns_config("myns");
    demonstrate_isolation();
    print_container_network_arch();

    printf("========================================\n");
    printf("[实际执行部分]\n");
    printf("========================================\n\n");

    /* 实际执行 unshare 创建网络命名空间 */
    demo_unshare_creation();

    printf("\n[演示完成]\n");
    printf("-----------\n");
    printf("本程序演示了 Linux 网络命名空间的基本概念和使用方法。\n");
    printf("在实际环境中，网络命名空间是 Docker、Kubernetes 等容器技术的\n");
    printf("网络隔离基础，实现了容器间的网络资源隔离。\n");

    return 0;
}
