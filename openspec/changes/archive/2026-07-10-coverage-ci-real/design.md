# S17 — Design（Coverage CI Real Wiring）

## 1. CI job 设计

```yaml
coverage-ubuntu:
  name: Coverage (Ubuntu/GCC+lcov)
  runs-on: ubuntu-22.04
  needs: test-engineering-ubuntu  # 复用 build

  steps:
    - uses: actions/checkout@v4
      with: { submodules: recursive }

    - name: Install lcov + python tools
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake build-essential lcov
        pip install pyyaml  # aggregate.py 依赖

    - name: Configure Engineering with Coverage
      working-directory: ./engineering
      run: |
        rm -rf build-cov
        cmake -B build-cov -S . \
          -DCMAKE_BUILD_TYPE=Debug \
          -DENABLE_COVERAGE=ON \
          -DBUILD_TESTING=ON

    - name: Build (with coverage flags)
      working-directory: ./engineering
      run: cmake --build build-cov --parallel 4

    - name: Run ctest (collect .gcda)
      working-directory: ./engineering/build-cov
      run: ctest --output-on-failure -j4 || true  # 不让 CI 红

    - name: Run lcov + filter + HTML
      working-directory: ./engineering
      run: bash scripts/coverage/run.sh || true

    - name: Generate current JSON
      working-directory: ./engineering
      run: |
        if [ -f coverage.info.filtered ]; then
          python3 scripts/coverage/aggregate.py coverage.info.filtered > coverage-current.json
        fi

    - name: Compare with baseline
      working-directory: ./engineering
      run: |
        python3 scripts/coverage/compare.py coverage-current.json docs/coverage-baseline.json > coverage-diff.md || true
        cat coverage-diff.md  # 显示在 CI log

    - name: Generate badge SVG
      working-directory: ./engineering
      run: |
        if [ -f coverage-current.json ]; then
          python3 scripts/coverage/badge.py coverage-current.json coverage-badge.svg
        fi

    - name: Upload artifacts
      uses: actions/upload-artifact@v4
      with:
        name: coverage-report
        path: |
          engineering/coverage-html/
          engineering/coverage.info
          engineering/coverage.info.filtered
          engineering/coverage-current.json
          engineering/coverage-diff.md
          engineering/coverage-badge.svg
```

## 2. 验证 V1-V4

| V | 检查 | 期望 |
|---|---|---|
| V1 | ci.yml YAML 合法 | python yaml.safe_load |
| V2 | run.sh 模拟跑通 | 工程层 build-cov 生成 .gcda |
| V3 | aggregate.py 解析 lcov | 输出 coverage-current.json |
| V4 | push 后 CI 跑通 | 看到 artifacts 上传 |

## 3. 不做

- ❌ Coverage 失败导致 CI 红
- ❌ PR 评论
- ❌ Codecov
