# function_scope 学习笔记

## 概念地图

C 语言的函数与作用域体系是"封装 + 信息隐藏"的根基。函数是 C 唯一的第一类代码组织单元——没有 class，没有 namespace，没有 package：

- **函数三要素**：
  - **声明（prototype/declaration）**：`int add(int a, int b);` ——告诉编译器函数签名，不分配代码
  - **定义（definition）**：`int add(int a, int b) { return a + b; }` ——分配代码
  - **调用（call）**：`add(3, 4)` ——按 prototype 检查类型，跳转到代码
- **存储类（Storage Class）四件套**：
  - `auto`（默认）：函数局部变量，栈分配
  - `static`（函数外）：文件作用域；静态存储期
  - `static`（函数内）：块作用域；静态存储期（**生命周期 = 程序，但作用域仍是函数内**）
  - `extern`：声明"在别的文件定义"，本身不分配存储
  - `register`：提示放寄存器（已废弃，现代编译器忽略）
- **链接属性（Linkage）三态**：
  - **外部链接**（默认）：全局变量/非 static 函数，整个程序可见
  - **内部链接**（`static`）：仅本 `.c` 文件可见
  - **无链接**（局部变量）：仅本函数/本块可见
- **作用域层级**：
  - **文件作用域**（全局）：`.c` 文件顶层
  - **函数作用域**：函数参数 + 函数体
  - **块作用域**：`{}` 内
  - **函数原型作用域**：prototype 参数列表（仅 C89）

## 踩坑记录

1. **prototype 不匹配**：调用 `add(3.14)` 但 prototype 是 `int add(int, int)` —— 编译器隐式转换，不报错但运行时可能错
2. **`static` 全局变量命名冲突**：两个 `.c` 文件都有 `static int count;` ——**完全合法且互不影响**，因为都是文件作用域
3. **`extern` 声明 vs 定义**：`extern int x;` 是声明，`int x;` 是定义（隐式 extern）。**重复定义 = 链接错误**
4. **块作用域遮蔽**：`int x = 1; { int x = 2; }` ——内层 `x` 遮蔽外层，GCC `-Wshadow` 警告
5. **未声明函数**：C89 默认返回 `int`，**C99+ 禁止未声明函数**——必须 prototype
6. **传值 struct 大对象**：100 字节的 struct 传值 = 100 字节拷贝。**大对象传指针**

## 工程对照（≥100 字硬约束）

函数与作用域在本仓库 `engineering/` 中体现为模块化设计的根基：

1. **头文件声明 vs 实现分离**：`engineering/include/db/catalog.h` 仅声明 `Catalog* catalog_create(void);` 等函数原型，`engineering/src/db/catalog.c` 提供实现——这是 C 项目的"接口与实现分离"标准模式。所有 `engineering/include/**.h` 头文件都是 .c 文件的"声明镜像"
2. **static 函数可见性控制**：`grep -rn "^static " engineering/src/db/storage/page/` 显示 disk.c/page.c 等有大量 static 函数——内部辅助函数不应污染外部命名空间。`static` 是 C 语言的"private"关键字
3. **extern 跨文件引用**：所有 `engineering/src/db/core/*.c` 共享 `engineering/src/db/core/errors.c` 定义的全局错误码表（`extern int error_codes[];`）。这种"中心化错误码 + 分布式引用"模式依赖 extern
4. **函数原型驱动 IDE 跳转**：在 VSCode/CLion 中打开 `engineering/src/db/storage/page/page.c`，Ctrl+点击 `page_read()` 直接跳转到 `engineering/include/db/storage/page/page.h` 的 prototype——这就是声明/分离带来的开发体验
5. **块作用域避免泄漏**：所有 `engineering/src/db/validator/sql_semantic.c` 的局部变量严格在最小作用域内声明——`for (int i = 0; ...)` 的 `i` 仅在 for 内可见，避免污染后续代码
6. **`static` 函数内联优化**：编译器对 `static` 函数 + 单次调用可激进内联——`engineering/src/algo-prod/sort/sort.c` 的小辅助函数都是 static，让编译器决定是否内联

学完本卡能动手的事：扫描 `learning/scaffold/` 下所有 main.c，把任何"全局变量"改为 `static` 或局部变量——消除模块间隐式依赖。