# c-lkm Makefile —— Kbuild 风格（递归调用内核构建系统）
#
# Linux 上使用方法：
#   make            # 编译 hello.ko（需要内核头文件 + gcc）
#   sudo insmod hello.ko
#   sudo dmesg | tail
#   sudo rmmod hello
#   sudo dmesg | tail
#   make clean
#
# Windows MinGW 上：直接跳过，提示需要 Linux。

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
  obj-m += hello.o
  KERNELDIR ?= /lib/modules/$(shell uname -r)/build
  PWD := $(shell pwd)

all:
	@echo "=== 编译 Linux 内核模块 hello.ko ==="
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

load: all
	@echo "=== 加载模块（需要 sudo） ==="
	sudo insmod hello.ko

unload:
	@echo "=== 卸载模块（需要 sudo） ==="
	sudo rmmod hello

dmesg:
	dmesg | tail -20

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c *.mod.o .*.cmd Module.symvers modules.order

else
  # 非 Linux 平台（Windows MinGW / macOS）—— graceful 降级

.PHONY: all load unload dmesg clean

all:
	@echo "[skip] c-lkm 需要 Linux 内核开发环境，当前 OS=$(UNAME_S)"
	@echo "[skip] 请在 Linux 上运行 make 编译 hello.ko"

load unload dmesg:
	@echo "[skip] c-lkm 需要 Linux，当前 OS=$(UNAME_S)"

clean:
	@echo "[skip] 无产物可清理"

endif