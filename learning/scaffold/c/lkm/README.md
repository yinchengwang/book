# lkm scaffold

Linux 内核模块（LKM）极简示例——`module_init/exit` + `printk` 8 级别 + 模块参数 + GPL 许可证。

## 文件结构

```
lkm/
├── hello.c        # 内核模块源码（含 printk 演示）
├── Makefile       # Kbuild 风格，Linux 编译 / MinGW 跳过
├── README.md      # 本文件
└── NOTES.md       # 学习笔记 + 工程对照
```

## 复现命令（Linux）

```bash
cd learning/scaffold/lkm

# 1. 编译（需要内核头文件：apt install linux-headers-$(uname -r) / yum install kernel-devel）
make

# 2. 加载模块（需要 root）
sudo insmod hello.ko
sudo dmesg | tail -15

# 3. 验证模块已加载
lsmod | grep hello

# 4. 卸载模块
sudo rmmod hello
sudo dmesg | tail

# 5. 清理
make clean
```

## 复现命令（Windows MinGW）

```bash
cd learning/scaffold/lkm
make

# 输出：
# [skip] lkm 需要 Linux 内核开发环境，当前 OS=Windows_NT (or MINGW64_NT-...)
# [skip] 请在 Linux 上运行 make 编译 hello.ko
```

## 预期输出（Linux dmesg | tail）

```
[init] hello: module loading...
[printk] KERN_EMERG   : system is unusable
[printk] KERN_ALERT   : action must be taken immediately
[printk] KERN_CRIT    : critical conditions
[printk] KERN_ERR     : error conditions
[printk] KERN_WARNING : warning conditions
[printk] KERN_NOTICE  : normal but significant
[printk] KERN_INFO    : informational
[printk] KERN_DEBUG   : debug-level messages
[param] Hello, world! (iteration 1/1)
[init] hello: module loaded successfully
... (rmmod 后)
[exit] hello: module unloading...
[exit] Goodbye, world!
[exit] hello: module unloaded
```

## 关键点

- **内核模块不能用 `<stdio.h>`**：没有 libc，没有 printf——用 `printk`
- **`printk` 8 个日志级别**：`KERN_EMERG`（0，最严重）到 `KERN_DEBUG`（7，最详细）。**只有 `< 4` 才默认显示在 console**
- **`MODULE_LICENSE("GPL")` 必需**：声明 GPL 否则某些内核符号（如 `printk`）用不了
- **`__init` / `__exit` 宏**：标记函数仅在初始化/卸载时存在，**模块加载完成后 `__init` 函数被释放内存**
- **Kbuild 编译机制**：`make -C /lib/modules/$(uname -r)/build M=$(pwd) modules`——递归调用内核构建系统
- **Windows 上完全不可编译**：内核模块只能在内核态编译，**只能在 Linux 上 make**

详见 NOTES.md 工程对照段。