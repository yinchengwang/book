# S12 Spec —— CI 双轨集成

## 1. CI 工作流结构

`.github/workflows/ci.yml` 必须含：

| Job | 触发 | 目标 |
|---|---|---|
| `build-engineering-ubuntu` | push/PR | `engineering/build` 配置+编译（gcc） |
| `test-engineering-ubuntu` | depends on build-engineering-ubuntu | `cd engineering/build && ctest` |
| `build-learning-ubuntu` | push/PR | `build-learning` 配置+编译（gcc） |
| `test-learning-ubuntu` | depends on build-learning-ubuntu | `cd build-learning && ctest` |
| `build-engineering-windows` | push/PR | Windows MinGW 编译 |

## 2. cmake 配置

- 工程层：`cmake -B engineering/build -S engineering -DBUILD_TESTING=ON`
- 学习层：`cmake -B build-learning -S learning -DENGINEERING_BUILD=OFF -DLEARNING_BUILD=ON -DBUILD_TESTING=ON`

## 3. 测试发现

- 工程层使用 `ctest --output-on-failure -j4`（走 CTestTestfile 注册的所有 target）
- 学习层使用 `ctest --output-on-failure`（走 LEARNING 学习层 leetcode_tests）

## 4. 不做（明确范围外）

- ❌ Coverage job（与 S2 旧债挂钩，留 S13+ 重新设计）
- ❌ clang-tidy（与 lcov 旧债类似）
- ❌ macOS build（仅 Ubuntu + Windows 双平台）
- ❌ 多 compiler matrix（仅 GCC）
