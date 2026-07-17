# undefined_behavior scaffold

5 类常见 UB 演示：signed overflow / null deref / OOB / use-after-free / sequence point。GCC `-Wsequence-point` 与 `-fsanitize=undefined` 拦截。

## 复现

```bash
cd learning/scaffold/undefined_behavior

# 1) Safe 模式（默认不触发 UB）
make
./ub_demo

# 2) UBSAN 拦截（Linux/WSL 推荐）
make debug
UBSAN_OPTIONS=halt_on_error=1 ./ub_ubsan_demo

# 3) 真触发 UB（必段错误）
./ub_demo run
```

## 预期输出（safe）

```
[UB] 编译此文件时加 -fsanitize=undefined,address 可拦截以下 UB
[UB] 默认跳过 UB 执行，需传 argv[1]=run 才能触发：

[safe] 全 safe 模式：UB 未触发；传 `undefined_behavior run` 真触发
```

## 本机现状

- `make`（默认 -O0 不开 sanitizers）：**PASS**
- `make debug`（带 -fsanitize=undefined）：mingw-w14.2 缺 `libubsan` 链接库；Linux/macOS 正常
- `run` 模式：会段错误，演示 UB 后果不可预期

## 关键点

- **UB 是编译期不抓、运行期不可预测的行为**：可能 print 错值、可能 segfault、可能默默工作
- **`-Wsequence-point`**：GCC 编译期就能抓到 `i++ + i++` 违例——这是静态防御
- **`-fsanitize=undefined`**：UBSAN 运行期拦截，告诉你"这里 UB 了"并 halt
- **`-fsanitize=address`**：ASAN 拦截内存越界与 use-after-free；本 demo safe 模式故意不带这两个
- **写 UB demo 的安全模式**：`if (run) {...}` 包住每个 UB 调用，避免误触发

详见 NOTES.md 工程对照段。
