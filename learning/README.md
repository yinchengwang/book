# Learning Track

学习轨道 —— β 学习日志。Obsidian 笔记、LeetCode 题解、教学数据结构、面试资料、CSDN 文章。

## 构建与测试

```bash
# Configure & build
cmake -B ../build/learning -S . -DBUILD_TESTING=ON
cmake --build ../build/learning --parallel 4

# Run tests
ctest --test-dir ../build/learning --output-on-failure
```

编译产物输出到 `build/learning/`，测试/运行产物输出到 `test-results/learning/`。

## Canonical 内容

| 目录 | 说明 |
|---|---|
| `notes/` | Obsidian vault（132+ 笔记） |
| `code-solutions/` | LeetCode/面试题解 + 分布式实验代码 |
| `ds-c/orig/` | 手撕数据结构教学版（C） |
| `algo-c/` | 教学算法 |
| `interview/` | 面试资料 |
| `exam/` | 考试认证 |
| `blog/` | 博客草稿 |
| `arkts/` | 鸿蒙 ArkTS |
| `articles/` | CSDN 长文 |
| `playground/` | 演示/练习代码（demo/demo_code/demo_projects） |
| `tools/` | 学习专用工具 |
| `misc/` | 杂项学习文件 |
| `scaffold/` | 学习脚手架（R5 卡片等） |
| `scripts/` | 学习专用脚本 |

## 资产归属

`learning/` 是以下所有内容的唯一 canonical 位置：
- 顶层 `Interview/`、`notes/`、`demo/`、`demo_code/`、`demo_projects/`——已迁入
- 顶层 `arkts/`、`blog/`、`exam/`——重复副本已合并
- 顶层 `csdn-article-content.md`、kanban/学习 misc 文件——已归入
- 学习专用工具——从根 `tools/` 拆分迁入 `learning/tools/`

不可引用 `engineering/` 任何路径。
