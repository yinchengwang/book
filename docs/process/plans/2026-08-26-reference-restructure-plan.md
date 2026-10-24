# 参考目录模型化重构与开源数据库补充实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按数据库模型重新组织 `reference/` 目录结构，补充主流及技术特色开源数据库参考源码，建立元数据清单，更新子模块与文档，并提供分阶段完整克隆指引。

**Architecture:** 将现有 `reference/open-source/` 平铺结构迁移到以模型分类为主的 `reference/<model>/` 目录，每个开源项目只保留一个主模型目录。新增候选项目按同一套目录与元数据规范纳入，新增项目数量不做硬性上限，按主流代表、技术特色、架构代表性三个层级分批完整克隆。同步更新 `.gitmodules`、仓库文档、学习资料与 README，保证现有引用不因路径迁移失效。

**Tech Stack:** Git submodule, Gitee/GitHub mirror, YAML catalog, Markdown documentation.

## Global Constraints

- `reference/` 目录仅作为只读参考源码使用，不参与工程或学习轨道构建，沿用 CLAUDE.md 与 `docs/architecture/dual-track.md` 的现有约束。
- 新增与迁移均使用完整 Git 克隆，不做浅克隆。
- 每个项目仅归入一个主模型目录，不得保留源码多副本。
- 现有子模块迁移采用直接修改 `.gitmodules` 中 path 的方式，保留当前镜像 URL。
- FAISS 主归 `vector/`，pgvector 主归 `extension/`，ANN-Benchmarks 主归 `benchmark/`。
- 模型目录采用英文规范命名，参考范围覆盖关系型/SQL、分布式与云原生、所有专用数据库模型。
- 优先纳入标准为活跃与生产级、技术特色、架构代表性；暂不纳入数量硬限制。
- 所有计划文档、README、提交信息均遵循项目中文文档与 OpenSpec 纪律。
- 候选项目先核对官方仓库、许可证、实际镜像地址、维护状态，再执行克隆。

---

### Task 1: 建立目录结构与元数据规范

**Files:**

- Create: `reference/README.md`
- Create: `reference/catalog.yml`
- Test: 人工核对目录可读性与 YAML 结构

- [ ] **Step 1: 创建模型目录索引文档**

```bash
mkdir -p reference/relational reference/distributed-sql reference/key-value reference/document reference/columnar reference/time-series reference/search reference/vector reference/graph reference/stream reference/embedded reference/extension reference/benchmark
```

- [ ] **Step 2: 编写目录总说明**

```bash
cat > reference/README.md << 'EOF'
# Reference 目录说明

本目录按数据库数据模型组织只读参考源码，不参与工程构建。

## 目录结构

- `relational/`：关系型数据库，包括传统 RDBMS 与嵌入式关系型数据库。
- `distributed-sql/`：分布式事务型、NewSQL、HTAP 与云原生数据库。
- `key-value/`：键值数据库与 LSM 存储系统。
- `document/`：文档数据库。
- `columnar/`：列式分析数据库与 OLAP 引擎。
- `time-series/`：时序数据库。
- `search/`：搜索数据库与全文检索引擎。
- `vector/`：向量数据库与向量检索库。
- `graph/`：图数据库。
- `stream/`：事件流与流数据库系统。
- `embedded/`：嵌入式数据库与本地分析引擎。
- `extension/`：数据库扩展、索引库与存储插件。
- `benchmark/`：数据库、检索或性能基准项目。

## 元数据规范

每个项目应在 `catalog.yml` 中维护以下字段：

- `name`：项目目录名
- `model`：模型目录
- `tier`：`core`、`representative`、`innovative`
- `repository`：官方仓库地址
- `mirror`：实际克隆地址
- `license`：开源许可证
- `status`：`active`、`maintained`、`legacy`
- `notes`：技术亮点与用途说明
EOF
```

- [ ] **Step 3: 创建初始元数据清单**

```bash
cat > reference/catalog.yml << 'EOF'
projects:
  - name: postgres
    model: relational
    tier: core
    repository: https://github.com/postgres/postgres.git
    mirror: https://github.com/postgres/postgres.git
    license: PostgreSQL
    status: active
    notes:
      - 关系型数据库典型实现
  - name: mysql
    model: relational
    tier: core
    repository: https://github.com/mysql/mysql-server.git
    mirror: https://gitee.com/yinchengwang_admin/mysql-server.git
    license: GPL-2.0
    status: active
    notes:
      - 主流关系型数据库
  - name: sqlite3
    model: relational
    tier: core
    repository: https://github.com/sqlite/sqlite.git
    mirror: https://github.com/sqlite/sqlite.git
    license: Public Domain
    status: active
    notes:
      - 嵌入式关系型数据库
  - name: openGauss
    model: relational
    tier: representative
    repository: https://gitee.com/opengauss/openGauss-server.git
    mirror: https://gitee.com/opengauss/openGauss-server.git
    license: MulanPSL-2.0
    status: active
    notes:
      - 企业级关系型数据库
  - name: redis
    model: key-value
    tier: core
    repository: https://github.com/redis/redis.git
    mirror: https://gitee.com/yinchengwang_admin/redis.git
    license: BSD-3-Clause
    status: active
    notes:
      - 键值数据库代表
  - name: elasticsearch
    model: search
    tier: core
    repository: https://github.com/elastic/elasticsearch.git
    mirror: https://gitee.com/yinchengwang_admin/elasticsearch.git
    license: SSPL
    status: active
    notes:
      - 搜索与文档检索引擎
  - name: chroma
    model: vector
    tier: representative
    repository: https://github.com/chroma-core/chroma.git
    mirror: https://github.com/chroma-core/chroma.git
    license: Apache-2.0
    status: active
    notes:
      - 向量数据库
  - name: milvus
    model: vector
    tier: core
    repository: https://github.com/milvus-io/milvus.git
    mirror: https://github.com/milvus-io/milvus.git
    license: Apache-2.0
    status: active
    notes:
      - 向量数据库
  - name: faiss
    model: vector
    tier: core
    repository: https://github.com/facebookresearch/faiss.git
    mirror: https://gitee.com/yinchengwang_admin/faiss.git
    license: MIT
    status: active
    notes:
      - 向量检索库
  - name: neo4j
    model: graph
    tier: core
    repository: https://github.com/neo4j/neo4j.git
    mirror: https://github.com/neo4j/neo4j.git
    license: GPL-3.0
    status: active
    notes:
      - 图数据库
  - name: pgvector
    model: extension
    tier: representative
    repository: https://github.com/pgvector/pgvector.git
    mirror: https://gitee.com/yinchengwang_admin/pgvector.git
    license: PostgreSQL
    status: active
    notes:
      - PostgreSQL 向量扩展
  - name: ann-benchmarks
    model: benchmark
    tier: representative
    repository: https://github.com/erikbern/ann-benchmarks.git
    mirror: https://gitee.com/yinchengwang_admin/ann-benchmarks.git
    license: MIT
    status: maintained
    notes:
      - 向量检索基准
EOF
```

- [ ] **Step 4: 提交目录与元数据框架**

```bash
git add reference/README.md reference/catalog.yml
git commit -m "docs(reference): 建立参考目录模型分类框架与元数据清单"
```

**Interfaces:**

- Consumes: 无前置依赖
- Produces: 新的目录骨架、说明文档与清单基线，后续任务均以此为模板

---

### Task 2: 编制候选项目清单并完成审核

**Files:**

- Modify: `reference/catalog.yml`
- Create: `reference/AUDIT.md`
- Test: 人工审核清单中的仓库地址、许可证与层级

- [ ] **Step 1: 补充各模型候选项目**

将下列候选项目逐项核对后追加到 `reference/catalog.yml`；不得只记录项目名称，必须同时填写 `repository`、`mirror`、`license`、`status`、`tier` 和 `notes`。仓库地址无法确认或许可证不清晰的项目先记录为 `status: review`，不得直接加入 `.gitmodules`。

| 模型 | 候选项目 |
|---|---|
| relational | MariaDB、Firebird、H2 |
| distributed-sql | CockroachDB、TiDB、YugabyteDB、OceanBase |
| key-value | RocksDB、LevelDB、FoundationDB、Badger、Pebble、Dragonfly |
| document | MongoDB、CouchDB、Couchbase、RavenDB、FerretDB |
| columnar | ClickHouse、Apache Doris、StarRocks、Apache Pinot、Apache Druid |
| time-series | InfluxDB、VictoriaMetrics、QuestDB、TDengine、Apache IoTDB |
| search | OpenSearch、Meilisearch、Typesense、Tantivy、Quickwit |
| vector | Qdrant、Weaviate、Vespa、LanceDB、Vald、usearch |
| graph | ArangoDB、JanusGraph、NebulaGraph、Memgraph、Kùzu |
| stream | Apache Kafka、Redpanda、Apache Pulsar、NATS |
| embedded | DuckDB、LMDB、BoltDB、Pklite |
| extension | Apache Arrow、DataFusion、Velox、Lance、HNSWlib |
| benchmark | TPC-H、TPC-DS、DB-Bench、VectorDBBench |

项目主模型只能填写一个；例如 DuckDB 归 `embedded`，CockroachDB/TiDB/YugabyteDB 归 `distributed-sql`，Kafka/Pulsar 归 `stream`。

- [ ] **Step 2: 建立审核清单**

```bash
cat > reference/AUDIT.md << 'EOF'
# Reference 候选项目审核表

| 项目 | 模型 | 官方仓库 | 实际镜像 | 许可证 | 状态 | 是否完整数据库 | 分层 | 审核结论 |
|---|---|---|---|---|---|---|---|---|
| postgres | relational | https://github.com/postgres/postgres.git | https://github.com/postgres/postgres.git | PostgreSQL | active | 是 | core | 通过 |
| mysql | relational | https://github.com/mysql/mysql-server.git | https://gitee.com/yinchengwang_admin/mysql-server.git | GPL-2.0 | active | 是 | core | 通过 |
| sqlite3 | relational | https://github.com/sqlite/sqlite.git | https://github.com/sqlite/sqlite.git | Public Domain | active | 是 | core | 通过 |
| openGauss | relational | https://gitee.com/opengauss/openGauss-server.git | https://gitee.com/opengauss/openGauss-server.git | MulanPSL-2.0 | active | 是 | representative | 通过 |
| redis | key-value | https://github.com/redis/redis.git | https://gitee.com/yinchengwang_admin/redis.git | BSD-3-Clause | active | 是 | core | 通过 |
| elasticsearch | search | https://github.com/elastic/elasticsearch.git | https://gitee.com/yinchengwang_admin/elasticsearch.git | SSPL | active | 是 | core | 通过 |
| chroma | vector | https://github.com/chroma-core/chroma.git | https://github.com/chroma-core/chroma.git | Apache-2.0 | active | 是 | representative | 通过 |
| milvus | vector | https://github.com/milvus-io/milvus.git | https://github.com/milvus-io/milvus.git | Apache-2.0 | active | 是 | core | 通过 |
| faiss | vector | https://github.com/facebookresearch/faiss.git | https://gitee.com/yinchengwang_admin/faiss.git | MIT | active | 否，检索库 | core | 通过 |
| neo4j | graph | https://github.com/neo4j/neo4j.git | https://github.com/neo4j/neo4j.git | GPL-3.0 | active | 是 | core | 通过 |
| pgvector | extension | https://github.com/pgvector/pgvector.git | https://gitee.com/yinchengwang_admin/pgvector.git | PostgreSQL | active | 否，扩展 | representative | 通过 |
| ann-benchmarks | benchmark | https://github.com/erikbern/ann-benchmarks.git | https://gitee.com/yinchengwang_admin/ann-benchmarks.git | MIT | maintained | 否，基准 | representative | 通过 |
EOF
```

- [ ] **Step 3: 标记所有候选项目的克隆状态与主模型**

在 `reference/catalog.yml` 为每个候选项目补齐 `mirror`、`license`、`status`、`tier` 与 `notes`，避免执行阶段临时查证。

- [ ] **Step 4: 提交审核基线**

```bash
git add reference/catalog.yml reference/AUDIT.md
git commit -m "docs(reference): 编制候选项目清单并完成审核基线"
```

**Interfaces:**

- Consumes: Task 1 的目录与元数据模板
- Produces: 经过审核的候选项目清单与审核表，供迁移与克隆任务使用

---

### Task 3: 迁移现有子模块路径

**Files:**

- Modify: `.gitmodules`
- Test: `git submodule status` 与目录核对

- [ ] **Step 1: 更新 `.gitmodules` 中现有 path**

按以下映射批量修改路径，保持 URL 不变：

- `reference/open-source/faiss` -> `reference/vector/faiss`
- `reference/open-source/redis` -> `reference/key-value/redis`
- `reference/open-source/pgvector` -> `reference/extension/pgvector`
- `reference/open-source/postgres` -> `reference/relational/postgres`
- `reference/open-source/sqlite3` -> `reference/relational/sqlite3`
- `reference/open-source/elasticsearch` -> `reference/search/elasticsearch`
- `reference/open-source/chroma` -> `reference/vector/chroma`
- `reference/open-source/milvus` -> `reference/vector/milvus`
- `reference/open-source/ann-benchmarks` -> `reference/benchmark/ann-benchmarks`
- `reference/open-source/neo4j` -> `reference/graph/neo4j`
- `reference/open-source/openGauss` -> `reference/relational/openGauss`
- `reference/open-source/mysql` -> `reference/relational/mysql`

可执行如下命令：

```bash
python3 - <<'PY'
from pathlib import Path
p = Path('.gitmodules')
text = p.read_text(encoding='utf-8')
replacements = {
    'reference/open-source/faiss': 'reference/vector/faiss',
    'reference/open-source/redis': 'reference/key-value/redis',
    'reference/open-source/pgvector': 'reference/extension/pgvector',
    'reference/open-source/postgres': 'reference/relational/postgres',
    'reference/open-source/sqlite3': 'reference/relational/sqlite3',
    'reference/open-source/elasticsearch': 'reference/search/elasticsearch',
    'reference/open-source/chroma': 'reference/vector/chroma',
    'reference/open-source/milvus': 'reference/vector/milvus',
    'reference/open-source/ann-benchmarks': 'reference/benchmark/ann-benchmarks',
    'reference/open-source/neo4j': 'reference/graph/neo4j',
    'reference/open-source/openGauss': 'reference/relational/openGauss',
    'reference/open-source/mysql': 'reference/relational/mysql',
}
for old, new in replacements.items():
    text = text.replace(old, new)
p.write_text(text, encoding='utf-8')
PY
```

- [ ] **Step 2: 同步 Git 模块元数据**

```bash
git rm --cached reference/open-source/faiss reference/open-source/redis reference/open-source/pgvector reference/open-source/postgres reference/open-source/sqlite3 reference/open-source/elasticsearch reference/open-source/chroma reference/open-source/milvus reference/open-source/ann-benchmarks reference/open-source/neo4j reference/open-source/openGauss reference/open-source/mysql
git submodule absorbgitdirs
```

- [ ] **Step 3: 验证迁移后的子模块状态**

```bash
git submodule status --reference reference
test ! -d reference/open-source
```

- [ ] **Step 4: 提交迁移结果**

```bash
git add .gitmodules reference
git commit -m "refactor(reference): 将现有子模块迁移到按模型分类的目录"
```

**Interfaces:**

- Consumes: Task 1 的目录结构与审核清单
- Produces: 按模型目录分布的现有子模块，供后续新增项目、文档更新和克隆指引使用

---

### Task 4: 新增第一阶段候选项目

**Files:**

- Modify: `.gitmodules`
- Modify: `reference/catalog.yml`
- Test: 新子模块可访问且目录存在

- [ ] **Step 1: 从清单中选择第一阶段项目**

在 `reference/catalog.yml` 中筛选出 `model` 为 `distributed-sql`、`key-value`、`document`、`columnar`、`time-series`、`search`、`vector`、`graph`、`stream`、`embedded` 的首批候选项目。目标是每个模型至少包含一个 `tier: core` 项目。

- [ ] **Step 2: 逐项添加子模块**

对每个入选项目执行：

```bash
git submodule add <mirror> reference/<model>/<name>
```

若 URL 无法访问，则退回审核表标注，并改用备用镜像或暂时移出本阶段清单。

- [ ] **Step 3: 更新元数据清单**

同步更新 `reference/catalog.yml`，为新增项目补齐 `repository`、`mirror`、`license`、`status`、`tier`、`notes`。

- [ ] **Step 4: 提交第一阶段新增项目**

```bash
git add .gitmodules reference
git commit -m "feat(reference): 新增第一阶段主流开源数据库参考子模块"
```

**Interfaces:**

- Consumes: Task 2 的审核清单与 Task 3 的目录基础
- Produces: 第一批新增子模块，覆盖主要专用与分布式数据库模型

---

### Task 5: 新增第二阶段特色项目与生态项目

**Files:**

- Modify: `.gitmodules`
- Modify: `reference/catalog.yml`
- Test: 特色项目子模块可访问，扩展与基准项目正确归类

- [ ] **Step 1: 筛选技术特色与架构代表项目**

在 `reference/catalog.yml` 中筛选 `tier: innovative` 与 `tier: representative` 的项目，重点纳入尚未主流但技术特色明显的实现。

- [ ] **Step 2: 添加第二阶段子模块**

```bash
git submodule add <mirror> reference/<model>/<name>
```

特别注意：

- `extension/` 只存放扩展、索引库与存储插件。
- `benchmark/` 只存放基准项目。
- `stream/` 只存放流数据库或事件存储，不将消息队列项目计入完整数据库数量。

- [ ] **Step 3: 更新审核表**

将第二阶段新增项目同步写入 `reference/AUDIT.md`，保持审核结论可追溯。

- [ ] **Step 4: 提交第二阶段成果**

```bash
git add .gitmodules reference
git commit -m "feat(reference): 新增技术特色项目与数据库生态项目"
```

**Interfaces:**

- Consumes: Task 4 的第一批子模块与 Task 2 的审核基线
- Produces: 补充后的完整项目集，包括技术特色项目、扩展项目和基准项目

---

### Task 6: 更新仓库文档与引用路径

**Files:**

- Modify: `CLAUDE.md`
- Modify: `docs/architecture/dual-track.md`
- Modify: `learning/scaffold/linux/unix_socket/NOTES.md`
- Modify: `learning/scaffold/ds/btree/NOTES.md`
- Modify: `learning/scaffold/c/git/NOTES.md`
- Modify: `third_part/sqlite3/CMakeLists.txt`
- Modify: `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_level.c`
- Modify: `docs/superpowers/plans/2026-07-16-hnsw-refactor-plan.md`
- Test: 在仓库中搜索 `reference/open-source` 应无残留引用

- [ ] **Step 1: 批量替换旧路径**

对上述文件执行替换：

```bash
python3 - <<'PY'
from pathlib import Path
files = [
    Path('CLAUDE.md'),
    Path('docs/architecture/dual-track.md'),
    Path('learning/scaffold/linux/unix_socket/NOTES.md'),
    Path('learning/scaffold/ds/btree/NOTES.md'),
    Path('learning/scaffold/c/git/NOTES.md'),
    Path('third_part/sqlite3/CMakeLists.txt'),
    Path('engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_level.c'),
    Path('docs/superpowers/plans/2026-07-16-hnsw-refactor-plan.md'),
]
old = 'reference/open-source/'
repls = {
    'reference/open-source/faiss': 'reference/vector/faiss',
    'reference/open-source/redis': 'reference/key-value/redis',
    'reference/open-source/pgvector': 'reference/extension/pgvector',
    'reference/open-source/postgres': 'reference/relational/postgres',
    'reference/open-source/sqlite3': 'reference/relational/sqlite3',
    'reference/open-source/elasticsearch': 'reference/search/elasticsearch',
    'reference/open-source/chroma': 'reference/vector/chroma',
    'reference/open-source/milvus': 'reference/vector/milvus',
    'reference/open-source/ann-benchmarks': 'reference/benchmark/ann-benchmarks',
    'reference/open-source/neo4j': 'reference/graph/neo4j',
    'reference/open-source/openGauss': 'reference/relational/openGauss',
    'reference/open-source/mysql': 'reference/relational/mysql',
}
for f in files:
    if not f.exists():
        continue
    text = f.read_text(encoding='utf-8')
    for oldp, newp in repls.items():
        text = text.replace(oldp, newp)
    text = text.replace(old, 'reference/')
    f.write_text(text, encoding='utf-8')
PY
```

- [ ] **Step 2: 更新 `docs/architecture/dual-track.md` 中的统计描述**

确认文件中“12 个 git submodule”等描述改为按模型统计，必要时补充新项目总数与目录说明。

- [ ] **Step 3: 搜索残留旧路径**

```bash
git grep -n "reference/open-source" || true
```

若仍有结果，继续定位并修正。

- [ ] **Step 4: 提交文档修正**

```bash
git add CLAUDE.md docs learning third_part/sqlite3/CMakeLists.txt engineering docs/superpowers/plans/2026-07-16-hnsw-refactor-plan.md
git commit -m "docs(reference): 更新仓库文档中的参考目录路径"
```

**Interfaces:**

- Consumes: Task 1 ~ Task 5 的目录与子模块结果
- Produces: 一致的文档引用，避免后续检索与维护冲突

---

### Task 7: 创建分阶段克隆与维护说明

**Files:**

- Modify: `reference/README.md`
- Create: `reference/fetch-manifest.txt`
- Test: 说明文档可读，清单文件与目录一致

- [ ] **Step 1: 补充分阶段克隆说明**

在 `reference/README.md` 中增加“分阶段克隆与维护”章节，说明：

- `catalog.yml` 是源码清单。
- 首次全量克隆前先确认磁盘容量。
- 可按模型目录分批克隆。
- `stream/` 中的部分系统不应被误算为传统数据库产品。
- `extension/` 与 `benchmark/` 属于生态项目。

- [ ] **Step 2: 生成全量克隆清单**

```bash
awk 'NR>2 {gsub(/[:"]/, ""); print $2 " " $4 " " $6}' reference/catalog.yml > reference/fetch-manifest.txt
```

若 `catalog.yml` 字段格式变化，则改用合适解析方式，确保输出为 `model name mirror`。

- [ ] **Step 3: 提交维护文档**

```bash
git add reference/README.md reference/fetch-manifest.txt
git commit -m "docs(reference): 添加分阶段克隆与维护说明"
```

**Interfaces:**

- Consumes: Task 1 ~ Task 6 的成果
- Produces: 可执行的维护说明与克隆清单，支撑后续阶段操作

---

### Task 8: 本地校验目录与清单一致性

**Files:**

- Test: `reference/` 下目录与 `reference/catalog.yml` 对齐
- Test: 未误将生态项目归类为完整数据库产品

- [ ] **Step 1: 列出已有目录**

```bash
find reference -mindepth 2 -maxdepth 2 -type d > reference/local-dirs.txt
cat reference/local-dirs.txt
```

- [ ] **Step 2: 比较清单与目录**

```bash
python3 - <<'PY'
from pathlib import Path
manifest = Path('reference/fetch-manifest.txt').read_text(encoding='utf-8').strip().splitlines()
missing = []
for line in manifest:
    parts = line.split()
    if len(parts) < 2:
        continue
    model, name = parts[0], parts[1]
    target = Path('reference') / model / name
    if not target.exists():
        missing.append(str(target))
if missing:
    raise SystemExit('Missing dirs: ' + ', '.join(missing))
PY
```

- [ ] **Step 3: 再次确认非数据库项目归类**

```bash
grep -E "faiss|pgvector|ann-benchmarks|pgvector|arrow|datafusion|velox|hnswlib" reference/catalog.yml
```

确认这些项目全部落在 `vector`、`extension`、`benchmark` 目录中，不在关系型、键值等目录中出现为“完整数据库产品”。

- [ ] **Step 4: 记录校验结果**

```bash
echo "目录校验完成" >> reference/AUDIT.md
git add reference/AUDIT.md reference/local-dirs.txt
git commit -m "chore(reference): 记录目录与清单校验结果"
```

**Interfaces:**

- Consumes: Task 1 ~ Task 7 的产出
- Produces: 本地一致性校验证据，为后续大范围克隆提供质量关口

---

### Task 9: 新增 OpenSpec 与变更记录

**Files:**

- Create: `openspec/changes/restructure-reference/README.md`
- Create: `openspec/changes/restructure-reference/tasks.md`
- Test: OpenSpec 记录与 CLAUDE.md 要求一致

- [ ] **Step 1: 建立变更目录**

```bash
mkdir -p openspec/changes/restructure-reference
```

- [ ] **Step 2: 编写变更说明**

```bash
cat > openspec/changes/restructure-reference/README.md << 'EOF'
# Reference 目录模型化重构

## 变更目标

将 `reference/` 从平铺的 `open-source/` 目录迁移到按数据模型分类的目录，并补充主流与技术特色开源数据库参考项目。

## 变更范围

- 新增模型目录与元数据清单
- 迁移现有子模块路径
- 补充新项目子模块
- 更新仓库文档与引用路径
- 建立分阶段克隆指引
EOF
```

- [ ] **Step 3: 编写任务清单**

```bash
cat > openspec/changes/restructure-reference/tasks.md << 'EOF'
## Tasks

- [x] 创建模型目录与元数据规范
- [x] 编制候选项目清单并审核
- [x] 迁移现有子模块路径
- [x] 新增第一阶段主流项目
- [x] 新增第二阶段特色与生态项目
- [x] 更新仓库文档与引用路径
- [x] 创建分阶段克隆与维护说明
- [x] 本地校验目录与清单一致性
EOF
```

- [ ] **Step 4: 提交 OpenSpec 记录**

```bash
git add openspec/changes/restructure-reference
git commit -m "docs(openspec): 新增 reference 目录重构变更记录"
```

**Interfaces:**

- Consumes: Task 1 ~ Task 8 的所有执行结果
- Produces: 与 OpenSpec 流程对齐的变更记录

---

### Task 10: 全量验证与最终提交

**Files:**

- Test: `git status`、`git submodule status`、`reference/README.md`、`reference/catalog.yml` 均正常

- [ ] **Step 1: 执行状态检查**

```bash
git status
git submodule status
```

- [ ] **Step 2: 核对文档最终版本**

```bash
grep -n "reference/relational" CLAUDE.md docs/architecture/dual-track.md reference/README.md
grep -n "reference/distributed-sql" reference/README.md reference/catalog.yml
```

- [ ] **Step 3: 进行最终提交**

```bash
git add .
git commit -m "chore(reference): 完成参考目录模型化重构与文档同步"
```

- [ ] **Step 4: 输出执行摘要**

记录以下信息到任务执行笔记或 PR 描述：

- 已迁移的现有项目数量
- 已新增的候选项目数量
- 仍待人工补录镜像或无法访问的项目
- 建议的后续批次克隆顺序

**Interfaces:**

- Consumes: Task 1 ~ Task 9 的全部变更
- Produces: 可交付的最终结果与可直接使用的维护入口
