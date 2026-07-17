# S12 — 设计文档（CI 双轨集成）

## 1. 当前 CI 状态

```
ci.yml (166 行, 4 jobs):
  - build-unix (ubuntu, macos, gcc/clang matrix)
  - build-windows (windows-latest)
  - coverage (ubuntu, lcov)  ← 引用已删除脚本
  - analyze (ubuntu, clang-tidy)
```

**问题**：
- 单根 `cmake -B build` 不能体现双轨
- coverage 引用 `engineering/scripts/coverage/compare.py`（已不存在）
- analyze job 跑 build 会因 ci.yml 里的 lcov 命令失败 build failed

## 2. 新 CI 结构

```yaml
jobs:
  build-engineering-ubuntu:   # 触发：push/PR 到 main, project
  build-engineering-windows: # 触发：push/PR 到 main, project
  build-learning-ubuntu:     # 触发：push/PR 到 main, project
  test-engineering-ubuntu:   # depends on build-engineering-ubuntu
  test-learning-ubuntu:      # depends on build-learning-ubuntu
```

## 3. 关键 cmake 命令

### engineering build

```bash
cmake -B engineering/build -S engineering \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON
cmake --build engineering/build --parallel 4
```

### learning build

```bash
cmake -B build-learning -S learning \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DENGINEERING_BUILD=OFF
cmake --build build-learning --parallel 4
```

### test commands

```bash
# 工程层（用 ctest 而非 all_tests 包装）
cd engineering/build && ctest --output-on-failure -j4

# 学习层
cd build-learning && ctest --output-on-failure -j4
```

## 4. 实现要点

### 4.1 actions/cache

```yaml
- uses: actions/cache@v4
  with:
    path: |
      engineering/build
      build-learning
      ~/.cache/ccache
    key: ${{ runner.os }}-cmake-${{ hashFiles('**/CMakeLists.txt') }}
```

### 4.2 简化的 Windows job

跳过 Windows 跑测试（学习层 pthread 等已验证 Linux），只 build：
- 工程层在 Windows (MinGW) build 通过即满足最低标准
- 学习层不在 Windows 上跑（避免双平台不必要复杂性）

## 5. 验证 V1-V4

| V | 检查 | 命令 |
|---|---|---|
| V1 | ci.yml YAML 语法合法 | `yamllint .github/workflows/ci.yml` |
| V2 | 工程层 build 命令在 CI 环境可跑 | 模拟：`cmake -B engineering/build -S engineering` 成功 |
| V3 | 学习层 build 命令在 CI 环境可跑 | 模拟：`cmake -B build-learning -S learning` 成功 |
| V4 | 提交到 project 时 CI 自动跑 | 推送后查看 .github/workflows runs |

## 6. 不做

- ❌ Coverage（用 lcov 输出报告）
- ❌ clang-tidy
- ❌ macOS build matrix
- ❌ Windows MSVC（仅 mingw）
- ❌ 自动 merge to main（保留人工 review）
