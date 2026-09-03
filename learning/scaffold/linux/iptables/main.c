/*
 * iptables_demo.c - iptables 防火墙规则演示
 *
 * 本程序演示 Linux iptables 防火墙的常见使用场景：
 *   - 基本包过滤规则
 *   - NAT 地址转换 (DNAT/SNAT)
 *   - 连接追踪状态 (ESTABLISHED, RELATED)
 *
 * 运行模式：
 *   - 默认纯模拟模式，打印规则而不实际执行
 *   - 传入 --apply 参数则调用 system() 执行真实命令（需 root 权限）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 是否执行真实 iptables 命令 */
static int dry_run = 1;

/* ============================================================
 * 辅助函数
 * ============================================================ */

static void print_separator(const char *title)
{
    printf("\n");
    printf("============================================================\n");
    printf("  %s\n", title);
    printf("============================================================\n");
}

/*
 * 执行 shell 命令
 * dry_run 模式下只打印，实际模式下调用 system()
 */
static void run_cmd(const char *cmd)
{
    if (dry_run) {
        printf("[模拟] %s\n", cmd);
    } else {
        printf("[执行] %s\n", cmd);
        int ret = system(cmd);
        if (ret != 0) {
            fprintf(stderr, "[警告] 命令返回非零退出码: %d\n", ret);
        }
    }
}

/* ============================================================
 * 演示 1：基本包过滤规则
 * ============================================================ */

static void demo_basic_filter(void)
{
    print_separator("基本包过滤规则");

    printf("\n[场景] 允许 HTTP/HTTPS 入站流量，丢弃其他入站流量\n\n");

    /* 允许已建立的连接继续通信（连接追踪） */
    run_cmd("iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT");

    /* 允许来自受信任网络的 SSH */
    run_cmd("iptables -A INPUT -p tcp -s 192.168.1.0/24 --dport 22 -j ACCEPT");

    /* 允许 HTTP 和 HTTPS */
    run_cmd("iptables -A INPUT -p tcp --dport 80 -j ACCEPT");
    run_cmd("iptables -A INPUT -p tcp --dport 443 -j ACCEPT");

    /* 默认拒绝策略 */
    run_cmd("iptables -P INPUT DROP");

    printf("\n[说明]\n");
    printf("  - -A INPUT: 将规则追加到 INPUT 链\n");
    printf("  - -m state --state ESTABLISHED,RELATED: 匹配已建立的连接\n");
    printf("  - -p tcp --dport: 匹配 TCP 协议指定端口\n");
    printf("  - -j ACCEPT/DROP: 动作为接受或丢弃\n");
}

/* ============================================================
 * 演示 2：NAT 地址转换
 * ============================================================ */

static void demo_nat(void)
{
    print_separator("NAT 地址转换");

    printf("\n[场景 1] SNAT - 内网主机访问外网时替换源 IP\n\n");

    run_cmd("iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o eth0 -j MASQUERADE");

    printf("\n[说明]\n");
    printf("  - -t nat: 指定 nat 表\n");
    printf("  - -A POSTROUTING: 在路由后修改数据包\n");
    printf("  - -s 192.168.1.0/24: 匹配源地址段\n");
    printf("  - -o eth0: 指定出口网络接口\n");
    printf("  - -j MASQUERADE: 自动获取出口 IP 作为源地址\n");

    printf("\n[场景 2] DNAT - 将外部请求转发到内网服务器\n\n");

    run_cmd("iptables -t nat -A PREROUTING -p tcp -i eth0 --dport 8080 -j DNAT --to-destination 192.168.1.100:80");

    printf("\n[说明]\n");
    printf("  - -A PREROUTING: 在路由前修改数据包（DNAT）\n");
    printf("  - -i eth0: 指定入口网络接口\n");
    printf("  - --dport 8080: 外部端口 8080\n");
    printf("  - --to-destination: 转发到内网 IP:PORT\n");
}

/* ============================================================
 * 演示 3：连接追踪状态
 * ============================================================ */

static void demo_connection_tracking(void)
{
    print_separator("连接追踪状态");

    printf("\n[场景] 仅允许来自已建立连接的入站流量\n\n");

    /* 允许已建立和相关的连接 */
    run_cmd("iptables -A INPUT -m conntrack --ctstate ESTABLISHED -j ACCEPT");
    run_cmd("iptables -A INPUT -m conntrack --ctstate RELATED -j ACCEPT");

    /* 允许新的入站连接请求 */
    run_cmd("iptables -A INPUT -m conntrack --ctstate NEW -p tcp --dport 22 -j ACCEPT");

    /* 允许 ICMP (ping) */
    run_cmd("iptables -A INPUT -p icmp -j ACCEPT");

    printf("\n[说明]\n");
    printf("  - ESTABLISHED: 已完成三次握手的连接\n");
    printf("  - RELATED: 关联现有连接的新连接（如 FTP 数据通道）\n");
    printf("  - NEW: 新建立的连接请求（如 TCP SYN）\n");
    printf("  - INVALID: 无效或无法识别的数据包\n");

    /* 查看连接追踪表（仅实际模式） */
    if (!dry_run) {
        printf("\n[查询连接追踪表]\n");
        system("cat /proc/net/nf_conntrack | head -5");
    } else {
        printf("\n[模拟] cat /proc/net/nf_conntrack | head -5\n");
    }
}

/* ============================================================
 * 演示 4：端口转发与透明代理
 * ============================================================ */

static void demo_port_forwarding(void)
{
    print_separator("端口转发与透明代理");

    printf("\n[场景] 将本机 80 端口流量重定向到 8080\n\n");

    run_cmd("iptables -t nat -A PREROUTING -p tcp --dport 80 -j REDIRECT --to-port 8080");

    printf("\n[场景] Docker 容器网络流量转发\n\n");

    run_cmd("iptables -t nat -A POSTROUTING -s 172.17.0.0/16 ! -o docker0 -j MASQUERADE");
    run_cmd("iptables -A FORWARD -i docker0 -o eth0 -j ACCEPT");
    run_cmd("iptables -A FORWARD -i eth0 -o docker0 -m state --state RELATED,ESTABLISHED -j ACCEPT");
}

/* ============================================================
 * 演示 5：常见服务防火墙配置
 * ============================================================ */

static void demo_service_rules(void)
{
    print_separator("常见服务防火墙配置");

    printf("\n[场景] Web 服务器最小化防火墙规则\n\n");

    /* 先清空 INPUT 链 */
    run_cmd("iptables -F INPUT");

    /* 允许本地回环 */
    run_cmd("iptables -A INPUT -i lo -j ACCEPT");

    /* SSH（限制源 IP） */
    run_cmd("iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 22 -j ACCEPT");

    /* HTTP/HTTPS */
    run_cmd("iptables -A INPUT -p tcp --dport 80 -j ACCEPT");
    run_cmd("iptables -A INPUT -p tcp --dport 443 -j ACCEPT");

    /* Ping（限流） */
    run_cmd("iptables -A INPUT -p icmp -m limit --limit 1/s -j ACCEPT");

    /* 日志 DROP（记录被丢弃的包） */
    run_cmd("iptables -A INPUT -m limit --limit 5/min -j LOG --log-prefix 'iptables-DROP: '");

    /* 默认拒绝 */
    run_cmd("iptables -P INPUT DROP");

    printf("\n[说明]\n");
    printf("  - -F INPUT: 清空 INPUT 链所有规则\n");
    printf("  - -i lo: 匹配本地回环接口\n");
    printf("  - -m limit: 限流模块，防止 DoS\n");
    printf("  - -j LOG: 将匹配记录到内核日志\n");
}

/* ============================================================
 * 演示 6：查看和保存规则
 * ============================================================ */

static void demo_save_list(void)
{
    print_separator("查看和保存规则");

    printf("\n[查看规则]\n");

    if (!dry_run) {
        system("iptables -L -n -v --line-numbers");
    } else {
        printf("[模拟] iptables -L -n -v --line-numbers\n");
        printf("\nChain INPUT (policy ACCEPT 0 packets, 0 bytes)\n");
        printf("num   pkts bytes target     prot opt in     out     source               destination\n");
        printf("\nChain FORWARD (policy ACCEPT 0 packets, 0 bytes)\n");
        printf("\nChain OUTPUT (policy ACCEPT 0 packets, 0 bytes)\n");
    }

    printf("\n[保存规则]\n");
    run_cmd("iptables-save > /etc/sysconfig/iptables");

    printf("\n[恢复规则]\n");
    run_cmd("iptables-restore < /etc/sysconfig/iptables");
}

/* ============================================================
 * 主函数
 * ============================================================ */

static void print_usage(const char *prog)
{
    printf("用法: %s [选项]\n", prog);
    printf("\n选项:\n");
    printf("  --apply   执行实际的 iptables 命令（需要 root 权限）\n");
    printf("  --help    显示帮助信息\n");
    printf("\n默认以模拟模式运行，只打印命令不实际执行。\n");
}

int main(int argc, char *argv[])
{
    printf("============================================================\n");
    printf("  iptables 防火墙规则演示\n");
    printf("============================================================\n");

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--apply") == 0) {
            dry_run = 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "未知选项: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (dry_run) {
        printf("\n[提示] 运行于模拟模式，使用 --apply 执行实际命令\n");
    } else {
        printf("\n[警告] 即将执行实际的 iptables 命令\n");
        printf("[提示] 请确保以 root 权限运行\n\n");
        sleep(2);  /* 给用户留出取消时间 */
    }

    /* 执行所有演示 */
    demo_basic_filter();
    demo_nat();
    demo_connection_tracking();
    demo_port_forwarding();
    demo_service_rules();
    demo_save_list();

    print_separator("演示完成");
    printf("\n要清理所有规则，可执行:\n");
    if (dry_run) {
        printf("  [模拟] iptables -F && iptables -X && iptables -t nat -F\n");
    } else {
        printf("  iptables -F && iptables -X && iptables -t nat -F\n");
    }
    printf("\n");

    return 0;
}
