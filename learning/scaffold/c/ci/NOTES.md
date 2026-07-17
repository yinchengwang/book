# ci 学习笔记

## 概念地图

CI（Continuous Integration，持续集成）= 代码合并时自动跑测试。CD（Continuous Deployment/Deployment）= 测试通过后自动部署：

- **CI 流水线三大阶段**：
  1. **编译** —— 失败立刻中止（fail fast）
  2. **测试** —— 单元测试 + 集成测试 + 覆盖率
  3. **静态分析** —— lint / cppcheck / flawfinder / clang-tidy
- **CI 系统三大件**：
  - **触发器**（trigger）：push / pull_request / schedule (cron) / workflow_dispatch
  - **执行环境**（runner）：GitHub-hosted（ubuntu/macos/windows）或 self-hosted
  - **产物**（artifact）：二进制 / 日志 / 覆盖率报告 / 包
- **GitHub Actions 核心概念**：
  - `jobs` —— 并行/串行的执行单元
  - `steps` —— job 内的有序步骤
  - `uses:` —— 复用别人写的 action（marketplace）
  - `run:` —— shell 命令
  - `matrix` —— 矩阵策略，自动生成多组合
  - `needs:` —— job 间依赖
  - `secrets` —— 密钥管理
- **YAML 基础**：缩进表示层级，`#` 注释，列表用 `-`，键值对 `key: value`
- **本地 CI**：在 CI 不可用时也能跑相同流水线——`ci.sh` 是 GitHub Actions 的"本地镜像"

## 踩坑记录

1. **YAML 缩进错误**：缩进 2 空格或 4 空格都可，但**同一文件必须一致**。tab 字符禁止
2. **`fail-fast: false`** 默认 true —— 单矩阵组合失败取消其他。CI 调试时**显式 false** 看全部组合
3. **缓存未失效**：`hashFiles` 没覆盖所有依赖文件时缓存永远生效，**改代码但 CI 看不到**
4. **actions 版本不兼容**：`@v3` `@v4` 不向后兼容——升级前查 changelog
5. **secret 误用**：`${{ secrets.GITHUB_TOKEN }}` 是 GitHub 自动注入的，**不要打印到 log**
6. **本地 CI 与远端 CI 不一致**：本地是 MinGW，远端是 ubuntu-latest——**`set -euo pipefail` 严格模式**让本地也能暴露问题

## 工程对照（≥100 字硬约束）

CI/CD 在本仓库 `engineering/` 中体现于双轨构建系统：

1. **`engineering/cmake/ProjectUtils.cmake` 的 `add_project_test()` 函数**：自动 glob `engineering/test/<lib>/*.cpp` 创建 gtest 可执行文件 + `add_test()` 注册到 CTest。这是 C 项目的"内置 CI"——`ctest` 自动发现并跑所有测试
2. **`engineering/CMakeLists.txt` 的 `all_tests` 自定义 target**：把所有测试包成一个 `make all_tests` 入口。这是工程轨 CI 的"本地代理"——开发者本地跑通后，CI 跑同一套测试
3. **`engineering/cmake/ProjectUtils.cmake` 的 `add_project_library()` 函数**：自动 glob `*.c`/`*.cpp` 创建静态库 + 编译选项统一注入。这是工程轨"编译策略中心化"的实现
4. **`engineering/apps/web/knowledge_hub/` 子项目**：有自己的 `.github/workflows/` CI 配置（Taro 3.6 + React 18 + Vite 5），跑 Node.js 端的 H5 + 微信小程序双端构建
5. **双轨守卫 `cmake/dual-track-guard.cmake`**：CMake 脚本检查双轨纪律——CI 跑时会强制验证"不混入对方轨道代码"。这是工程轨 CI 的**业务规则测试**
6. **`openspec/` 变更管理 = 流程化 CI**：每个 OPSX 变更的 proposal → tasks → spec → archive 形成"提案→实施→归档"的工作流，类似 GitLab Flow 的 CI/CD pipeline
7. **`engineering/Dockerfile` + `docker-compose.yml`**：Docker 多阶段构建镜像 + 服务编排——是 CD（持续部署）的载体，CI 通过后构建镜像并部署

学完本卡能动手的事：把 `learning/scaffold/` 下所有 32 张卡的编译命令封装成统一 `learn-ci.sh`，按 R5-R9 顺序编译验证，作为本地学习轨 CI。