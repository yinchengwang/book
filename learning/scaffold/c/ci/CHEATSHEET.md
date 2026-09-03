# CI/CD 与 YAML 速查表

## GitHub Actions 触发器

```yaml
on:
  push:
    branches: [ main ]
    paths: ['src/**']              # 仅 src/ 变化触发
  pull_request:
    branches: [ main ]
  schedule:
    - cron: '0 2 * * *'           # 每天凌晨 2 点
  workflow_dispatch:               # 手动触发
    inputs:
      env:
        description: 'Environment'
        required: true
        default: 'dev'
```

## 矩阵策略

```yaml
strategy:
  fail-fast: false
  matrix:
    os: [ubuntu-latest, macos-latest]
    compiler: [gcc, clang]
    version: ['11', '12']
    exclude:
      - os: macos-latest
        compiler: gcc
    include:
      - os: ubuntu-latest
        compiler: gcc
        version: '14'
        coverage: true
```

## 常用 Actions

| Action | 用途 |
|--------|------|
| `actions/checkout@v4` | 检出代码 |
| `actions/cache@v3` | 缓存目录 |
| `actions/upload-artifact@v3` | 上传产物 |
| `actions/download-artifact@v3` | 下载产物 |
| `actions/setup-python@v4` | 安装 Python |
| `actions/setup-node@v3` | 安装 Node |
| `actions/setup-go@v4` | 安装 Go |

## Job 依赖

```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    steps: [...]

  test:
    needs: build                  # 串行：等 build 成功
    runs-on: ubuntu-latest
    steps: [...]

  lint:
    runs-on: ubuntu-latest         # 并行：与 build/test 同时
    steps: [...]
```

## 环境变量与密钥

```yaml
env:                              # job 级环境变量
  CC: gcc
  CFLAGS: -O2

jobs:
  build:
    steps:
      - run: echo "$CC $CFLAGS"
      - env:
          MY_TOKEN: ${{ secrets.MY_TOKEN }}    # step 级
        run: curl -H "Authorization: $MY_TOKEN" ...
```

## 表达式语法

```yaml
${{ github.event_name }}          # push / pull_request
${{ github.ref }}                 # refs/heads/main
${{ github.sha }}                 # commit SHA
${{ github.actor }}               # 用户名
${{ matrix.compiler }}            # 矩阵当前值
${{ steps.build.outputs.binary }} # 上一步 output
${{ secrets.GITHUB_TOKEN }}       # 内置 token
${{ env.CC }}                     # 环境变量
```

## YAML 基础

```yaml
# 注释
key: value                        # 标量
key:                              # 对象
  nested: value
list:                             # 列表
  - item1
  - item2
multiline: |                      # 字面量字符串（保留换行）
  line 1
  line 2
multiline: >                      # 折叠字符串（换行 → 空格）
  line 1
  line 2
```

## Bash CI 脚本模板

```bash
#!/usr/bin/env bash
set -euo pipefail                  # 严格模式：失败即停

echo "=== Step 1 ==="
gcc -Wall -Wextra -O2 -std=c11 -o build/app main.c

echo "=== Step 2 ==="
./build/app

echo "=== Step 3 ==="
command -v cppcheck >/dev/null 2>&1 \
    && cppcheck --enable=all main.c \
    || echo "[skip] cppcheck 不可用"

echo "=== All passed ==="
```

## CI 流水线模式

| 模式 | 何时用 | 示例 |
|------|--------|------|
| 编译 → 测试 | 简单 C 项目 | `make && make test` |
| 编译 → 测试 → 静态分析 → 打包 | 生产项目 | + cppcheck + tar |
| 矩阵多编译器 | 跨平台库 | gcc × clang × msvc |
| 多 stage（dev/staging/prod） | SaaS | deploy stage 链 |