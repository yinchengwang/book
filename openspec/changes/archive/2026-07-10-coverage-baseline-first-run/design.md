# S24 — Design（Coverage Baseline First Run）

## 1. 工具链检测

CI Ubuntu（apt 装 lcov）：用 `lcov --capture ...`
本地 Windows（无 lcov）：用 `gcov` + 自写解析

S24 同时提供两种模式，由脚本自动检测。

## 2. gcov-only 模式（Windows / 本地）

```bash
# Step 1: build with coverage
cmake -B engineering/build-cov -S engineering \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_COVERAGE=ON
cmake --build engineering/build-cov --parallel 4

# Step 2: 跑 ctest（生成 .gcda）
cd engineering/build-cov && ctest --output-on-failure -j4

# Step 3: gcov 收集
cd engineering
mkdir -p coverage-gcov
find build-cov -name "*.gcda" -exec gcov {} \; >/dev/null
# gcov 生成 .gcov 文件

# Step 4: 聚合
python3 scripts/coverage/aggregate-gcov.py > docs/coverage-baseline.json
```

## 3. aggregate-gcov.py 设计

- 读 `coverage-gcov/*.gcov`（gcov 输出）
- 按 `engineering/src/<lib>/` 一级目录聚合
- 输出 `{modules: [{name, lines_pct, ...}]}`

## 4. 不做

- ❌ HTML 报告（无 lcov）
- ❌ genhtml（依赖 lcov）
- ❌ Codecov 上传
