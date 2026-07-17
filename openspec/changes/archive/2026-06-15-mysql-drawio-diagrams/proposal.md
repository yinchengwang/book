## Why

阅读雷达项目中 MySQL/Redis 图解文章目前只有纯 ASCII 文本图，缺少数据库产品级的 drawio 架构图（如 Buffer Pool 内存分布、行锁/Gap锁/临键锁、MVCC 流程等）。小林coding 已公开了高质量的 drawio 图解，但我们的文章未能引用这些专业图片，导致知识深度和视觉表达差距明显。

## What Changes

- 研究小林coding（xiaolincoding.com）的 MySQL/Redis 图解内容，提取知识盲区和高质量图示
- 用 drawio 格式重新绘制所有核心架构图（XML 格式可直接保存为 `.drawio` 文件）
- 将 drawio 图片托管到 CDN（参考 `cdn.xiaolincoding.com` 路径结构）
- 在现有 MD 学习文章中嵌入专业架构图，补齐知识盲区
- 覆盖 MySQL 24 个知识点 + Redis 15 个知识点的图解增强

## Capabilities

### New Capabilities
- `drawio-diagrams`: 数据库产品级架构图的绘制、托管和嵌入规范（MySQL/Redis/PostgreSQL）
- `illustration-enhancement`: 将 drawio 图片嵌入 MD 学习文章的渲染增强

## Impact

- `data/learn-deep/illustrate/mysql/` — 23 篇 MD 文章增强（新增/替换 ASCII 图为 drawio 图片）
- `data/learn-deep/illustrate/redis/` — 5 篇 MD 文章增强
- `data/learn-deep/illustrate/postgres/` — 后续扩展
- `data/illustrations/` — 新增目录存放 `.drawio` 源文件
- CDN 路径设计：`cdn.xiaolincoding.com/gh/xiaolincoder/<product>/<topic>/<image>.drawio.png`
