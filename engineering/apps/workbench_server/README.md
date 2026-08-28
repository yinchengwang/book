# workbench-server 独立二进制

Phase 4 Task 1 拆分：workbench 业务从 web-server 拆出，独立二进制（端口 5801）。
Phase 7 Task 20（spec §14 Path C）：独立 DB 文件（workbench.db）。

## 独立 DB 文件（spec §14 Path C）

### 背景

spec §14 风险表第 1 项：「与 todo-app SQLite 锁竞争 | 工作台响应变慢 | 缓解：workbench 走独立 connection + SQLite WAL mode」。

- **WAL mode**（`db_pool.c:21`）已就位
- **独立 connection**（Phase 4 拆进程）已就位
- **独立 DB 文件**（Phase 7 Task 20）= 本次新增

### 默认 db_path

workbench-server 默认打开 `workbench.db`（与 web-server 默认 `todo-app.db` 物理隔离）。

### 首次部署

如果是从 todo-app.db 升级到 workbench.db：

```bash
# 1. 停止 workbench-server
pkill workbench-server

# 2. 跑一次性 ETL 把 11 张 workbench 表迁移到新 DB
bash scripts/workbench-etl.sh
# 备份原 todo-app.db 到 todo-app.db.bak.<unix_ts>（不自动删除）

# 3. 启动 workbench-server（自动建空 workbench.db + CREATE IF NOT EXISTS 幂等）
./build/engineering/workbench-server
```

### 环境变量覆盖

```bash
# 本地 dev 用临时 DB
WORKBENCH_DB_PATH=/tmp/dev-workbench.db ./build/engineering/workbench-server

# 集成测试
WORKBENCH_DB_PATH=:memory: ./build/engineering/workbench-server  # 如 db_pool 支持
```

### 双 DB 一致性

两端通过 `cards.touched_by` 列对账（spec §10.3）：

```bash
# 单端口扫
bash scripts/workbench-reconcile.sh todo-app.db
bash scripts/workbench-reconcile.sh workbench.db

```
