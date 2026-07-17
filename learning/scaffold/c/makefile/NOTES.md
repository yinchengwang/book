# makefile 学习笔记

## 概念地图

GNU Make 是 POSIX 平台事实标准 build 工具。本卡的 Makefile 覆盖 R5 主题的八件套：

1. **变量**：`CC CFLAGS LDFLAGS`，带 `?=` 给命令行 override
2. **`:=` vs `=`**：立即展开 vs 递归展开；前者更可预测
3. **模式规则**：`%.o: %.c` 一行代替每文件一条显式规则
4. **自动依赖生成**：`-MMD -MP` 让 GCC 输出 `.d` 文件；`-include *.d` 让 make 把它们纳入依赖图
5. **`.PHONY`**：声明非文件 target，避免与同名目录文件冲突
6. **`$(addprefix)/$(addsuffix)`**：批量字符串变换
7. **Order-only prerequisite**：`mkdir -p $(OBJDIR)` 用 `|` 分隔，确保 build/ 存在但不触发 rebuild
8. **`make -n` 干跑**：验证 Makefile 不真副作用执行——CI 必备

Makefile 与本仓库主构建的关系：本仓库主项目用 CMake（`cmake/ProjectUtils.cmake`），独立项目（project/）、scaffold 子目录、tests 都改走 Makefile。本卡教学 Makefile，是 R5 「工程回路卡」——前 8 张 scaffold 都靠 Makefile 编译，§9 把 Makefile 自身当学习对象。

## 踩坑记录

1. **本机 MinGW 没 make 命令**：实测只能用 `gcc` 直接走通。本 README 给出"无 make"的等效 gcc 命令，**这是真实工程现实**——无 make 也能理解 Makefile 的每一行在做什么。
2. **`main` 多重定义错误**：第一次写 demo 链上 test.o 出 `multiple definition of main`。修法：demo 与 test 拆成两个独立二进制，互不链。这是 Makefile 设计本身的反馈——**`SRC_DEMO` 与 `SRC_TEST` 分两套**。
3. **mingw 下 `WinMain` 缺失**：用 mingw 链接 `.c` 默认假设是 GUI 程序，需要 `-mconsole`。本 demo 没用 `-mconsole` 但成功——因为我们的 main.c 与 test.c 都是 `int main(void) {...}` 标准 console entry。
4. **`extern run_test_body()` 在 main.c**：原本想让 demo 接受 `argv[1]='t'` 调测试函数，但 demo 链 test.o 引入 main 冲突。修法：demo 与 test 完全独立，命令行参数判断从 main.c 删除。
5. **`-MMD -MP` 写在哪**：必须写在 `CFLAGS` 里，不能单独传 `-MMD`，否则 make 看不到依赖文件；本 demo 写 `CFLAGS ?= -Wall -Wextra -O2 -std=c11 -MMD -MP`。
6. **`$(OBJDIR)/*.d` 是 glob**：make 3.x 接受 `wildcard` 但 `-include` 也接受；建议先 `$(shell mkdir -p $(OBJDIR))` 或 order-only `$(OBJDIR)` target——本 demo 用后者。

## 工程对照（≥100 字硬约束）

Makefile 在工程侧有以下直接迁移价值：

1. **`learning/scaffold/<card>/Makefile`**：R5 八张卡都用 Makefile（除了 pthread 用了 pthread 标志）。本卡刷过后读者能为每张卡写出 `make / make test / make clean` 标准三件套。
2. **`engineering/src/algo-prod/CMakeLists.txt` 与 Makefile 共存**：PG 风格工程里 Makefile 与 CMake 是**互补关系**——CMake 管跨平台工具链，Makefile 管零碎小工具。本卡刷过后读者识别 make 与 cmake 各自适用场景。
3. **`engineering/scripts/` 下的脚本构建**：仓库根 `scripts/` 下有自写脚本（`test-summary.py` 等），构建工具链是 Makefile。这些 Makefile 可加 `-MMD` 让脚本依赖正确的脚本入参校验。本卡提供入口。
4. **`rag-remote-index-backend` Python SDK 的 setup.py vs Makefile**：Python 项目用 setuptools；底层可调 make 编译 C 扩展（hnsw.c → .so）。本卡提供 Makefile 骨架，未来 RAG 项目加 `make ext` 编译 HNSW C 扩展。
5. **phony target 应用 `learning/scaffold/R5-HANDOFF.md` 与 `NOTES.md`**：把"读 R5 进展"作为 .PHONY `make check-handoff` 之类的 target——这是 Makefile 表达力的现代用法。
6. **`/usr/bin/install` vs `cp`**：`install -m 0755` 设文件权限，比 cp 后 chmod 更原子；本 demo 的 install 目标示范这条工程习惯。

**学完本卡能动手的事**：

- 给 `engineering/src/algo-prod/quantizer.c` 配独立 Makefile，含测试与 bench
- 给 R5 8 张卡加 `make all/run/test/clean` 标准接口
- 写 `tools/check-style/Makefile`，用 `find + xargs -n1 clang-format` 实现增量 lint

这是 R5 闭环的最后一张卡——**把前 8 张的编译节奏收口到一套 Makefile 习惯**，让后续 R6+ 加卡成本极低。
