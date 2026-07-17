# Engineering Track

工程轨道 —— α 工程作品集。PG 风格存储引擎、MiniVecDB、apps、RAG、SDK、CI/CD、Docker。

## 构建与测试

```bash
# Configure & build
cmake -B ../build/engineering -S . -DBUILD_TESTING=ON
cmake --build ../build/engineering --parallel 4

# Run tests
ctest --test-dir ../build/engineering --output-on-failure
ctest --test-dir ../build/engineering -R <name> --output-on-failure

# Coverage (GCC/lcov)
cmake -B ../build/engineering -S . -DENABLE_COVERAGE=ON
```

编译产物输出到 `build/engineering/`，测试/运行产物输出到 `test-results/engineering/`。

## Canonical 内容

| 目录 | 说明 |
|---|---|
| `src/` | 库代码（db/, algo/, cpp/, redis/） |
| `include/` | 公共头文件 |
| `test/` | gtest 测试 |
| `apps/` | 独立应用（db_driver, games, tools, web） |
| `rag/` | RAG 系统 |
| `sdk/` | 多语言 SDK |
| `tools/` | 工程专用工具 |
| `cmake/` | CMake 工具（ProjectUtils.cmake） |
| `test_data/` | 测试夹具 |

## 资产归属

`engineering/` 是以下所有内容的唯一 canonical 位置：
- 顶层 `apps/`、`rag/`、`sdk/`、Docker 文件——已迁入
- 根目录 `include/` 中工程头文件——已审计归入 `engineering/include/`
- 根目录 `test_data/` 中复用测试夹具——已迁入
- 工程专用工具——从根 `tools/` 拆分迁入 `engineering/tools/`

不可引用 `learning/` 任何路径。
