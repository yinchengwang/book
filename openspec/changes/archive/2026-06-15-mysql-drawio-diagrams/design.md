## Context

阅读雷达的 MySQL/Redis 图解文章目前只有纯 ASCII 文本图，缺乏数据库产品级的架构可视化。小林coding 已公开了高质量的 drawio 图解（GitHub 仓库 `xiaolincoder/mysql`），但我们的内容角度不同——面向 C/C++ 学习者，而非 Java 后端面试。我们需要自己的 drawio 图，而不是直接复制。

当前项目状态：
- 314 个 MD 文件已生成（23 MySQL + 5 Redis + 11 PostgreSQL + 数据库通用 11 + 其他）
- `learn.html` 已有 `.diagram-block` CSS 样式支持图片居中
- 无 drawio 源文件或 CDN 托管方案

## Goals / Non-Goals

**Goals:**
- 为 MySQL 24 个知识点绘制至少 20 张专业 drawio 架构图
- 为 Redis 15 个知识点绘制至少 10 张专业 drawio 架构图
- 所有图片以 drawio XML 格式保存源文件 + PNG 格式用于嵌入
- 在 MD 文章中正确嵌入 drawio 图片，支持移动端适配

**Non-Goals:**
- 不复制小林的内容（保持 C/C++ 学习者视角）
- 不做在线 drawio 编辑器（仅生成静态文件）
- 不改变现有 MD 文章的文字内容（只增强图片）

## Decisions

### 决策 1：drawio XML 格式 vs Mermaid
**选择：drawio XML 格式**
- 理由：drawio 支持复杂的数据库架构图（多层嵌套、箭头标注、颜色编码），Mermaid 在复杂度和美观度上不足
- 替代方案考虑：Mermaid（简单但不够专业）、PlantUML（语法复杂）、手绘（不可维护）

### 决策 2：图片托管位置
**选择：`data/illustrations/` 目录 + CDN 路径**
- 理由：与现有 `data/learn-deep/illustrate/` 分离，drawio 源文件和 PNG 分开存放
- 路径设计：`data/illustrations/mysql/buffer-pool/buffer_pool_diagram.drawio`
- CDN 路径：`cdn.xiaolincoding.com/gh/xiaolincoder/mysql/<topic>/<image>.drawio.png`

### 决策 3：图片导出方式
**选择：drawio 桌面版手动导出**
- 理由：drawio 桌面版导出 PNG 质量最高，可调整 DPI 和缩放
- 自动化方案考虑：drawio headless（Windows 上不稳定，需要 GUI 环境）

### 决策 4：配色方案
**选择：按产品区分主题色**
- MySQL：蓝色系（#4477C2 为主色）
- Redis：红色系（#DC382D 为主色）
- PostgreSQL：深蓝色系（#336791 为主色）
- 理由：帮助用户快速识别数据库产品

## Risks / Trade-offs

| Risk | 影响 | 缓解措施 |
|------|------|---------|
| drawio 学习曲线 | 初期效率低 | 先用小林已有的图作为参考模板，逐步掌握 |
| 图片文件膨胀 | MD 文件变大 | PNG 压缩到 500KB 以内，drawio 源文件独立存放 |
| CDN 路径 404 | 图片无法加载 | 本地 fallback 到 `data/illustrations/` 相对路径 |
| 移动端渲染问题 | 图片溢出屏幕 | CSS 限制 `max-width: 900px; width: 100%; height: auto;` |

## Migration Plan

1. **Phase 1**：MySQL 20 张核心图（Buffer Pool、锁、MVCC、Redo Log、Undo Log、B+ 树索引、SQL 执行流程等）
2. **Phase 2**：Redis 10 张核心图（内存模型、AOF/RDB、主从复制、Sentinel、Cluster、数据结构）
3. **Phase 3**：PostgreSQL 扩展（后续批次）
4. **嵌入阶段**：更新 MD 文章，将 drawio 图片嵌入对应知识点

## Open Questions

1. CDN 图片上传由谁负责？（是否需要部署 OSS？）
2. 是否需要对已有 MD 文章做批量替换（将 ASCII 图替换为 drawio 图片）？
3. 是否需要在 learn.html 中增加图片懒加载（`loading="lazy"`）优化首屏性能？
