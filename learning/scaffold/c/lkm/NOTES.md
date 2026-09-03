# lkm 学习笔记

## 概念地图

Linux 内核模块（Loadable Kernel Module, LKM）是**运行时可加载到内核空间的代码**——它不是用户态程序，没有 libc、没有 printf、没有 main 函数：

- **三件套宏**：
  - `MODULE_LICENSE("GPL")` —— 必须声明，否则部分内核符号不可用
  - `MODULE_AUTHOR/ DESCRIPTION / VERSION` —— 元信息
  - `module_init(init_fn)` / `module_exit(exit_fn)` —— 注册入口/出口函数
- **`__init` / `__exit` 宏**：标记函数仅在初始化/卸载阶段存在，模块加载完成后 `__init` 函数被回收内存（节省内核空间）
- **`printk` vs `printf`**：
  - printk 写入内核 ring buffer（`dmesg` 命令读取）
  - printk **永不失败**——内核态 panic 时仍能输出最后几条日志
  - 8 个日志级别 `KERN_EMERG`~`KERN_DEBUG`，**只有 `< KERN_WARNING (4)` 才默认显示 console**
- **模块参数**：`module_param(var, type, perm)` 让模块加载时可传参，例如 `insmod hello.ko count=5 name="alice"`
- **Kbuild 编译**：`make -C /lib/modules/$(uname -r)/build M=$(pwd) modules` ——内核源码树提供 Makefile 规则，模块源码只写 `obj-m += hello.o`
- **加载/卸载命令**：
  - `insmod xxx.ko` —— 加载（需要 root）
  - `rmmod xxx` —— 卸载（不要 .ko 后缀）
  - `modprobe xxx` —— 智能加载，自动解决依赖
  - `lsmod` —— 列出已加载模块

## 踩坑记录

1. **缺内核头文件**：编译时报 `linux/init.h: No such file`。Ubuntu 装 `linux-headers-$(uname -r)`，CentOS 装 `kernel-devel`，**头文件版本必须与运行内核一致**
2. **`printk` 消息未显示**：`dmesg` 看不到——检查日志级别（`< 4` 才显示 console），或 `dmesg -n 8` 临时打开所有级别
3. **`rmmod` 时 "module in use"**：模块被其他模块引用。先 `rmmod` 依赖方，或 `rmmod -f` 强制
4. **GPL 污染**：模块用了 GPL-only 符号（如 `request_firmware`），但声明 `"Proprietary"`——**insmod 失败 + taint 内核**
5. **Windows 上不可编译**：内核模块**只能在 Linux 内核源码树**编译。MinGW 缺 `<linux/init.h>` 等
6. **段错误 = 内核 panic**：模块空指针解引用会让整个系统死锁。**测试用 QEMU/虚拟机**

## 工程对照（≥100 字硬约束）

Linux 内核模块是 OS 内核开发的核心范式。本仓库双轨纪律（不修改 `engineering/`）下，工程对照转向：

1. **Linux 5.10 内核 `kernel/printk/printk.c`**：内核 printk 的实现定义了 8 个日志级别常量（`LOGLEVEL_EMERG=0` 到 `LOGLEVEL_DEBUG=7`），与本卡 `KERN_EMERG`~`KERN_DEBUG` 一一对应。这是 Linux 内核日志基础设施的"标准答案"
2. **`include/linux/printk.h`**：所有 `KERN_*` 宏都定义在这里——`#define KERN_SOH "\001"` + `"0"`-`"7"` 的字符串拼接。这是 C 预处理器宏的"教科书级用法"，对应本卡 preprocessor 卡的 `##` 拼接主题
3. **`include/linux/module.h`**：所有 `MODULE_*` 宏 + `module_init/exit` + `module_param` 在此定义。Linux 5.10 通过 `__attribute__((section("...")))` 把模块元信息放到 ELF 特殊段
4. **`kernel/module.c` 的 `load_module()`**：用户态 `init_module` 系统调用进入此函数——用 `finit_module` 或 `init_module` 加载 .ko 文件、解析 `module_param`、调用 `module_init` 注册的函数
5. **仓库 `docs/storage-architecture.md`**：提到 `io_uring` 是 kernel-bypass I/O——绕过系统调用直接在内核态提交 I/O 请求。这是高级 LKM 用法
6. **`engineering/src/db/storage/page/disk.c` 的数据库存储**：所有页面读写都通过 `open/read/write/lseek` 系统调用进入内核。**用户态 C 是内核的"高级客户端"**，LKM 是内核的"扩展点"

学完本卡能动手的事：在 Linux 虚拟机里 `make` 编译 hello.ko + `insmod` + `dmesg` + `rmmod`，验证模块全生命周期。Windows 上直接跳到 Card 8。