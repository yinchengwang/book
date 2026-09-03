# CDN 设计 学习笔记

## 核心概念

- **边缘节点**: 靠近用户的缓存节点，全球部署
- **中间源**: 区域汇聚节点，减轻源站压力
- **缓存命中率**: CDN 核心指标，一般目标 95%+
- **动态加速 (DSA)**: 实时探测最优回源路径

## CDN 关键策略

| 内容类型 | 缓存时间 | Cache-Control | 说明 |
|----------|----------|---------------|------|
| 图片/CSS/JS | 7 天 | immutable | 版本化 URL |
| HTML | 5 分钟 | must-revalidate | 动态内容 |
| API JSON | 1 分钟 | no-cache | 需验证 |
| 敏感数据 | 不缓存 | no-store | 安全要求 |

## 工程对照

`engineering/src/db/core/db_server.c` 中实现的数据库服务器协议可以类比 CDN 的"源站"概念——源站负责数据的权威存储和复杂处理。`engineering/include/db/storage_engine.h` 中定义的存储引擎抽象（`storage_engine_ops_t`）提供了多模态数据的统一访问接口，这类似于 CDN 的"中间层抽象"——边缘节点不需要知道源站的具体存储细节。`engineering/src/db/rel/rel.c` 中的 Relation 扫描接口（`table_scan`、`table_insert` 等）对应 CDN 的读/写操作路由。`engineering/` 轨中的网络服务器使用了 epoll（Linux）和 select（跨平台）的事件驱动模型，这对应 CDN 边缘节点的并发连接处理。`engineering/apps/web/knowledge_hub/` 是 React 前端应用，其静态资源（JS/CSS/图片）在实际部署中正是通过 CDN 分发的，静态资源的版本化（文件名 hash）对应 CDN 的缓存刷新策略。

## 面试要点

1. CDN 不只是静态内容加速，也支持动态内容
2. 缓存刷新策略直接影响用户体验
3. 多 CDN 架构可避免单 CDN 故障
