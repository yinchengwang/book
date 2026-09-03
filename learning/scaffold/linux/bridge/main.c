/**
 * bridge_demo.c - Linux 网桥配置演示
 *
 * 本程序演示 Linux 网桥（Bridge）的配置流程，包括：
 *   - 使用 brctl 命令创建和管理网桥
 *   - 使用 ip link 命令配置网桥设备
 *   - 二层转发（MAC 地址学习与转发）原理说明
 *   - VLAN 在网桥中的配置方式
 *
 * 编译: gcc -std=c11 -Wall -O2 -o bridge_demo bridge_demo.c
 * 运行: sudo ./bridge_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 辅助函数：打印分隔线
 * ============================================================ */
static void print_separator(const char *title)
{
    printf("\n");
    printf("============================================================\n");
    printf("  %s\n", title);
    printf("============================================================\n");
}

/* ============================================================
 * 1. 使用 brctl 命令创建和管理网桥
 * ============================================================ */
static void demo_brctl_commands(void)
{
    print_separator("brctl 命令演示 - 网桥管理");

    /* 说明：brctl 是 bridge-utils 包提供的传统网桥管理工具
     * 在新系统中推荐使用 ip link 替代，但 brctl 仍广泛使用 */

    printf("\n[步骤 1] 安装 bridge-utils（如果未安装）\n");
    printf("  $ apt-get install bridge-utils    # Debian/Ubuntu\n");
    printf("  $ yum install bridge-utils        # RHEL/CentOS\n");

    printf("\n[步骤 2] 创建网桥 br0\n");
    printf("  $ brctl addbr br0    # 创建名为 br0 的网桥\n");

    printf("\n[步骤 3] 将物理网卡添加到网桥\n");
    printf("  $ brctl addif br0 eth0    # 将 eth0 加入 br0\n");
    printf("  $ brctl addif br0 eth1    # 将 eth1 加入 br0\n");

    printf("\n[步骤 4] 启用网桥转发功能\n");
    printf("  $ echo 1 > /proc/sys/net/ipv4/ip_forward    # 开启 IP 转发\n");

    printf("\n[步骤 5] 设置网桥的 STP（生成树协议）\n");
    printf("  $ brctl stp br0 on    # 开启 STP，防止环路\n");
    printf("  $ brctl setbridgeprio br0 32768    # 设置网桥优先级\n");
    printf("  $ brctl setfd br0 15            # 设置转发延迟\n");

    printf("\n[步骤 6] 查看网桥状态\n");
    printf("  $ brctl show              # 列出所有网桥及端口\n");
    printf("  $ brctl showmacs br0      # 查看 MAC 地址表\n");
    printf("  $ brctl showstp br0       # 查看 STP 状态\n");

    printf("\n[步骤 7] 从网桥移除接口\n");
    printf("  $ brctl delif br0 eth0    # 将 eth0 从 br0 移除\n");

    printf("\n[步骤 8] 删除网桥\n");
    printf("  $ brctl delbr br0         # 删除 br0 网桥\n");
}

/* ============================================================
 * 2. 使用 ip link 命令配置网桥
 * ============================================================ */
static void demo_ip_link_commands(void)
{
    print_separator("ip link 命令演示 - 现代网桥配置");

    /* 说明：iproute2 的 ip link 命令是现代 Linux 推荐的方式
     * 支持更丰富的功能和更好的集成 */

    printf("\n[步骤 1] 创建网桥设备\n");
    printf("  $ ip link add br0 type bridge    # 创建网桥 br0\n");

    printf("\n[步骤 2] 将接口添加到网桥\n");
    printf("  $ ip link set eth0 master br0    # 将 eth0 加入 br0\n");
    printf("  $ ip link set veth0 master br0   # 将 veth0 加入 br0\n");
    printf("  $ ip link set eth1 master br0   # 将 eth1 加入 br0\n");

    printf("\n[步骤 3] 启用网桥和端口\n");
    printf("  $ ip link set br0 up      # 启用网桥设备\n");
    printf("  $ ip link set eth0 up     # 启用物理端口\n");
    printf("  $ ip link set veth0 up    # 启用虚拟端口\n");

    printf("\n[步骤 4] 从网桥移除接口\n");
    printf("  $ ip link set eth0 nomaster    # 从 br0 移除 eth0\n");

    printf("\n[步骤 5] 设置网桥属性\n");
    printf("  $ ip link set br0 type bridge stp_state 1    # 开启 STP\n");
    printf("  $ ip link set br0 type bridge forward_delay 15\n");
    printf("  $ ip link set br0 type bridge priority 32768\n");

    printf("\n[步骤 6] 查看网桥信息\n");
    printf("  $ ip link show type bridge    # 查看所有网桥\n");
    printf("  $ bridge link                  # 查看网桥端口\n");
    printf("  $ bridge fdb show             # 查看 MAC 转发表\n");
    printf("  $ bridge vlan show            # 查看 VLAN 表\n");

    printf("\n[步骤 7] 删除网桥\n");
    printf("  $ ip link del br0              # 删除网桥及其从属接口\n");
}

/* ============================================================
 * 3. 二层转发原理说明
 * ============================================================ */
static void explain_layer2_forwarding(void)
{
    print_separator("二层转发原理 - MAC 地址学习与转发");

    printf("\n[网桥工作原理]\n\n");
    printf("1. MAC 地址学习\n");
    printf("   - 网桥学习每个端口看到的源 MAC 地址\n");
    printf("   - 建立 MAC 地址表：MAC -> Port 映射\n");
    printf("   - 每次收到帧，更新对应 MAC 的老化时间\n");
    printf("   - 默认老化时间 300 秒（可通过 aging_time 设置）\n\n");

    printf("2. 帧转发决策\n");
    printf("   - 目标 MAC 在表中 -> 从对应端口转发\n");
    printf("   - 目标 MAC 不在表中 -> 泛洪到所有端口（除来源）\n");
    printf("   - 广播帧 (FF:FF:FF:FF:FF:FF) -> 泛洪到所有端口\n\n");

    printf("3. 环路避免 - STP（生成树协议）\n");
    printf("   - 检测网络环路\n");
    printf("   - 阻塞冗余路径\n");
    printf("   - 故障时自动启用备份路径\n");
    printf("   - 端口状态: Blocking -> Listening -> Learning -> Forwarding\n\n");

    printf("4. VLAN 在网桥中的配置\n");
    printf("   - 每个端口可配置 VLAN ID\n");
    printf("   - 同一 VLAN 内可通信\n");
    printf("   - 不同 VLAN 间需三层路由\n");
    printf("   - 支持 Access Port 和 Trunk Port\n\n");

    printf("[典型网桥拓扑]\n");
    printf("  +--------+     +--------+\n");
    printf("  | Host A |-----|  eth0  |\n");
    printf("  +--------+     |        |\n");
    printf("                 |  br0   | <-- 网桥\n");
    printf("  +--------+     |        |\n");
    printf("  | Host B |-----|  eth1  |\n");
    printf("  +--------+     +--------+\n");
}

/* ============================================================
 * 4. VLAN 网桥配置示例
 * ============================================================ */
static void demo_vlan_bridge(void)
{
    print_separator("VLAN 网桥配置示例");

    printf("\n[创建 VLAN 网桥的步骤]\n\n");

    printf("1. 创建网桥\n");
    printf("   $ ip link add br100 type bridge    # VLAN 100 网桥\n");
    printf("   $ ip link add br200 type bridge    # VLAN 200 网桥\n\n");

    printf("2. 创建 VLAN 子接口\n");
    printf("   $ ip link add eth0.100 link eth0 type vlan id 100\n");
    printf("   $ ip link add eth0.200 link eth0 type vlan id 200\n\n");

    printf("3. 将 VLAN 接口加入对应网桥\n");
    printf("   $ ip link set eth0.100 master br100\n");
    printf("   $ ip link set eth0.200 master br200\n\n");

    printf("4. 启用所有设备\n");
    printf("   $ ip link set eth0 up\n");
    printf("   $ ip link set eth0.100 up\n");
    printf("   $ ip link set eth0.200 up\n");
    printf("   $ ip link set br100 up\n");
    printf("   $ ip link set br200 up\n\n");

    printf("5. 为网桥分配 IP（作为 VLAN 网关）\n");
    printf("   $ ip addr add 192.168.100.1/24 dev br100\n");
    printf("   $ ip addr add 192.168.200.1/24 dev br200\n\n");

    printf("[Trunk Port 配置]\n");
    printf("   # 允许所有 VLAN 通过的 Trunk 口\n");
    printf("   $ ip link set eth0 master br0\n");
    printf("   $ bridge vlan add vid 100-200 dev eth0 pvid untagged\n");
}

/* ============================================================
 * 主函数 - 演示入口
 * ============================================================ */
int main(void)
{
    printf("============================================================\n");
    printf("  Linux 网桥配置演示程序\n");
    printf("  模式: 纯模拟输出（不实际修改系统配置）\n");
    printf("============================================================\n");

    /* 说明：此程序为模拟模式，只打印预期行为
     * 实际配置需要使用 sudo 执行上述命令 */

    printf("\n提示: 运行本程序前请确保有 root 权限\n");
    printf("      实际配置请手动执行输出的命令\n");

    /* 演示各个功能模块 */
    demo_brctl_commands();
    demo_ip_link_commands();
    explain_layer2_forwarding();
    demo_vlan_bridge();

    print_separator("演示结束");
    printf("\n实际配置时请按顺序执行上述命令。\n");
    printf("清理环境请使用:\n");
    printf("  $ ip link del br0\n");
    printf("  $ ip link del br100\n");
    printf("  $ ip link del br200\n");
    printf("\n");

    return 0;
}
