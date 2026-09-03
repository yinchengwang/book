/* lkm scaffold — Linux 内核模块（hello world）
 *
 * 本文件是 Linux 内核模块（LKM）的极简示例。
 * **无法在 Windows MinGW 上编译**——需要 Linux + 内核头文件 + GCC。
 *
 * 在 Linux 上编译/加载/卸载：
 *   make                       # 编译 hello.ko
 *   sudo insmod hello.ko       # 加载模块
 *   sudo dmesg | tail          # 查看 printk 输出
 *   sudo rmmod hello           # 卸载模块
 *   sudo dmesg | tail
 *
 * 演示 5 段（仅 printk，不实际挂载驱动）：
 *   [init]      — module_init 入口
 *   [printk]    — 8 个日志级别演示
 *   [param]     — 模块参数传递
 *   [license]   — GPL 许可证声明（必需——非 GPL 模块污染内核）
 *   [exit]      — module_exit 清理
 *
 * ⚠️ 内核模块不能用 printf()，必须用 printk()：
 *   KERN_EMERG/KERN_ALERT/KERN_CRIT/KERN_ERR/KERN_WARNING
 *   KERN_NOTICE/KERN_INFO/KERN_DEBUG
 */

#include <linux/init.h>       /* __init / __exit macros */
#include <linux/module.h>     /* 所有模块必需 */
#include <linux/kernel.h>     /* printk + KERN_* 级别 */
#include <linux/param.h>      /* module_param */

MODULE_LICENSE("GPL");                            /* 必需 */
MODULE_AUTHOR("book learner");
MODULE_DESCRIPTION("lkm scaffold — hello world kernel module");
MODULE_VERSION("1.0");

/* === [param] 模块参数：insmod hello.ko count=5 name="alice" === */
static int count = 1;
static char *name = "world";
module_param(count, int, S_IRUGO);                 /* S_IRUGO = 只读权限 */
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(count, "How many times to greet");
MODULE_PARM_DESC(name, "Whom to greet");

/* === [init] 模块加载入口 === */
static int __init hello_init(void)
{
    int i;

    printk(KERN_INFO "[init] hello: module loading...\n");

    /* === [printk] 8 个日志级别演示 === */
    printk(KERN_EMERG   "[printk] KERN_EMERG   : system is unusable\n");
    printk(KERN_ALERT   "[printk] KERN_ALERT   : action must be taken immediately\n");
    printk(KERN_CRIT    "[printk] KERN_CRIT    : critical conditions\n");
    printk(KERN_ERR     "[printk] KERN_ERR     : error conditions\n");
    printk(KERN_WARNING "[printk] KERN_WARNING : warning conditions\n");
    printk(KERN_NOTICE  "[printk] KERN_NOTICE  : normal but significant\n");
    printk(KERN_INFO    "[printk] KERN_INFO    : informational\n");
    printk(KERN_DEBUG   "[printk] KERN_DEBUG   : debug-level messages\n");

    /* === [param] 使用模块参数 === */
    for (i = 0; i < count; i++) {
        printk(KERN_INFO "[param] Hello, %s! (iteration %d/%d)\n",
               name, i + 1, count);
    }

    printk(KERN_INFO "[init] hello: module loaded successfully\n");
    return 0;     /* 返回 0 表示加载成功，非 0 表示失败 */
}

/* === [exit] 模块卸载入口 === */
static void __exit hello_exit(void)
{
    printk(KERN_INFO "[exit] hello: module unloading...\n");
    printk(KERN_INFO "[exit] Goodbye, %s!\n", name);
    printk(KERN_INFO "[exit] hello: module unloaded\n");
}

module_init(hello_init);
module_exit(hello_exit);