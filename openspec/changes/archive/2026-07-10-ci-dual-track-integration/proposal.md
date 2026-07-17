# S12 — CI Dual-Track Integration

## What Changes

`.github/workflows/ci.yml` 当前**预 S1 双轨架构**：

```
job: build-unix / build-windows / coverage / analyze
     ↓
cmake -B build   ← 单根构建，不支持 -DLEARNING_BUILD=ON
ctest -j4        ← 仅能跑工程层测试
coverage         ← 引用已删除的 engineering/scripts/coverage/compare.py 等
```

S1 + S1' + S5 + S6 + S7 + S8 + S9 完成后，现状：

- 工程层：~256 tests passing（含 vector_index 42 + 已知 ~56 其他模块）
- 学习层：158 tests passing（leetcode）
- 单独 build：`cmake -B engineering/build -S engineering`（默认全部）
- 单独 build：`cmake -B build-learning -S learning`（默认全部，但 -DENGINEERING_BUILD=OFF）

S12 重写 CI 让双轨独立可验：

**变更内容**：

1. **删除旧的 coverage + analyze jobs**（引用已删文件）
2. **增加 4 个并行 jobs**：
   - `build-engineering`：Ubuntu GCC，跑工程层
   - `build-learning`：Ubuntu GCC，跑学习层
   - `test-engineering`：跑工程层 ctest（~256 tests）
   - `test-learning`：跑学习层 ctest（158 tests）
3. **Windows job**：仍 build，但只 build-engineering（学习层依赖 X11 等已有 Linux 假设但 Windows 也能跑，跳过简化）
4. **Cache**：用 `actions/cache@v4` 缓存 cmake build dir 加速

## Why

**α 价值（工程作品集）**：
- CI 跑过即"项目工程化"的硬指标
- 256 tests 自动跑 = 防止 S8 / S9 类的回归 bug
- 替换旧 CI 引用已删除脚本的失败 build

**β 价值（学习日志）**：
- 学习层 158 tests 在 CI 自动跑，跨平台验证 leetcode/ds 实现正确性

**前置依赖**：
- S1-S9 已让双轨独立可编译 + 跑通测试
- S10 specs 治理已使架构稳定
- S11 phase9 完成后学习层完整

## Scope

**包含**：
- 重写 `.github/workflows/ci.yml`：4 jobs (build-eng, build-learning, test-eng, test-learning) + Windows build job
- 用 actions/cache 缓存
- 不引入 coverage（覆盖率是 S2 已失败的债；S12 不做）

**不包含**：
- ❌ coverage job（已失败债，留给后续 S13+）
- ❌ clang-tidy analyze job（一致性问题）
- ❌ 多平台（macOS, Windows MSVC）—— 只跑 Ubuntu GCC + Windows mingw

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| Windows job 找不到 cmake | 中 | 改用 `msys2/setup-msys2` 或 `ilammy/msvc-dev-cmd` 简化 |
| 学习层 ctest 跑挂 | 低 | V2 验证（V3 已在本地通过） |
| 双轨 build 都在 main PR 上跑导致时间翻倍 | 中 | 并行 jobs，配置 matrix |

## 不做（明确范围外）

- ❌ 覆盖率报告（coverage）—— 旧脚本已删
- ❌ clang-tidy 静态分析
- ❌ macOS build（移到后续）
- ❌ 多 compiler matrix（仅 GCC 跑）
