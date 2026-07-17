# c11_features scaffold

C11 关键新特性合集：`_Generic` 编译期类型分支、`designated initializer`、C11 `static_assert` 关键字、anonymous union in struct、`_Alignas / _Alignof`。

## 复现

```bash
cd learning/scaffold/c11_features
gcc -Wall -Wextra -O2 -std=c11 -o c11_demo main.c
./c11_demo
```

## 预期输出

```
int: 42
float: 3.14
double: 2.718280
char *: hello
[init] host=127.0.0.1 port=8080 workers=4
[anon-union] p1.kind=0 data.len=5 data='hello'
[anon-union] p2.kind=1 ctrl.code=200
[align]   _Alignof(buf) = 16
```

warning `_Generic 分支不匹配` 是 demo 故意展示"分支类型不匹配，编译器仍能编译但 printf 走变参"——读 `man _Generic` 完整规范。

## 关键点

- **`_Generic` 是编译期宏**：返回类型选哪个 printf，**调用点行为**
- **`designated initializer`**：`{.port=8080, .host="..."}` 比位置初始化安全——字段重排不会无声断
- **`static_assert`**：C11 关键字（不要写 `assert.h` 的 `assert()`）
- **匿名 union**：`struct { union { int a; float b; }; };` 直接 `s.a` 访问
- **`_Alignas(N)`**：保证 N 字节对齐，与 SIMD / cache line 互动
- **C11 vs C99**：本仓库全部按 `-std=c11` 编译；`.clang-format` 已允许 C11 风格

详见 NOTES.md 工程对照段。
