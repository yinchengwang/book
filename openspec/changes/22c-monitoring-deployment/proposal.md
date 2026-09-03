# Task 22-C: 监控 / 部署 / 复制（占位提案）

**变更名称：** workbench 微服务监控与部署
**创建日期：** 2026-08-23
**状态：** 调研阶段（research subagent 运行中）
**关联 spec：** `docs/superpowers/specs/2026-08-17-workbench-micro-service-design.md` §14 / §16

---

## 背景

Phase 7 Task 22-A（多实例负载均衡）+ Task 22-B（JWT secret 旋转）已完成。当前 workbench 微服务架构已有：

- ✅ web-server + workbench-server 双进程
- ✅ nginx upstream 多实例负载均衡
- ✅ JWT 多密钥 + grace period + rotate-secret 端点
- ✅ SQLite WAL + busy_timeout

**缺监控 / 部署 / 复制基础设施：**

1. 无 metrics 暴露 — Prometheus 无法拉取指标
2. 无 Grafana dashboard — 运维无可视化
3. docker-compose 仅 MiniVecDB 单服务，无 nginx / workbench / web-server
4. 无 k8s manifest — 生产部署缺标准

---

## What Changes

### 必做项（12 文件）
**新建：**
- `engineering/apps/common/metrics.h` + `metrics.c` — Prometheus text exposition format 手写实现（~300 行，无第三方依赖）
- `engineering/apps/web/server/src/metrics_handler.c` + `.h` — `GET /metrics` HTTP handler
- `engineering/apps/workbench_server/src/workbench_metrics.c` + `.h` — workbench 专用指标（DB 慢查询 / card 操作）
- `engineering/docker-compose.workbench.yml` — 多服务部署（nginx + workbench-server + web-server + prometheus + grafana）
- `engineering/prometheus/prometheus.yml` — scrape config
- `engineering/grafana/provisioning/` — datasources + dashboards provisioning
- `engineering/grafana/dashboards/workbench.json` — 6 面板 dashboard
- `engineering/docs/workbench-deployment.md` — standalone / docker-compose / k8s 三模式部署文档
- `engineering/k8s/` — k8s manifest starter 模板

**修改：**
- `engineering/apps/web/server/src/main.c` — 注册 `GET /metrics` 端点 + metrics 中间件
- `engineering/apps/workbench_server/src/main.c` — 注册 `GET /metrics` + DB 埋点
- `scripts/workbench-acceptance.sh` — 新增 phase22c smoke（6 步验证）

---

## Non-Goals（明确范围）

- ❌ 不引入第三方 tracing（Jaeger / Zipkin）
- ❌ 不改鉴权 / rotate 逻辑（Task 22-B 已落地）
- ❌ 不动 workbench 业务路由（card_handler / notes_api 等）
- ❌ 不引入 OpenTelemetry SDK（保持轻量）

---

## 当前状态

- ✅ OPSX change 目录已建（`openspec/changes/22c-monitoring-deployment/`）
- ✅ 所有 12 个文件已实施完成
- ✅ 编译通过（web-server + workbench-server）
- ✅ Workbench gtest 30/30 通过
- ✅ /metrics 端点已注册（web-server + workbench-server）
- ✅ Prometheus metrics 手写实现（无需第三方依赖）
- ✅ docker-compose.workbench.yml 多服务部署配置
- ✅ prometheus.yml + grafana provisioning
- ✅ k8s manifest 模板
- ✅ workbench-deployment.md 三模式部署文档
- ✅ workbench-acceptance.sh phase22c smoke 验证
- **状态：✅ Task 22-C 实施完成**