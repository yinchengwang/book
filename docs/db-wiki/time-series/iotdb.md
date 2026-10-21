# IoTDB Wiki（工业 IoT 时序数据库）

> 参考版本：IoTDB 当前 main 分支（commit `a10bc6ba7e`，2026 era），TsFile 列存文件 + 多层架构 + Apache 顶级项目，工业协议原生支持（OPC UA / Modbus / MQTT）。

## 一、项目定位

Apache IoTDB（Internet of Things Database）是 **Apache 软件基金会顶级项目**，由 **清华大学软件学院 + Apache IoTDB 社区** 主导的 **工业 IoT 原生时序数据库**，以 **"TsFile 自研列存格式 + 端边云协同 + 工业协议原生 + 双层引擎"** 著称，是 **工业互联网、车联网、能源、轨道交通、智能制造** 等场景的核心选择。它把 **TsFile（自研列存文件格式）、IoTDB Engine（数据节点）、AINode（AI 内置节点）、Pipe（数据同步）、Trigger（实时触发器）、UDTF（用户自定义函数）** 集成到完整 Apache 生态，并原生支持 **OPC UA / Modbus / MQTT / CAN / DDS 等工业协议**。

- **痛点**：
  - 工业 IoT 设备种类多、频率差异大（1Hz-100kHz）；
  - 工业协议众多（OPC UA、Modbus、MQTT、CAN、DDS、Profinet）；
  - 边端资源受限（CPU、内存、磁盘都很小）；
  - 边云协同：边端预处理 + 云端深度分析；
  - 工业场景对**乱序写入、数据丢失恢复、长保留**有严格要求；
  - 需要 **工业 AI 集成（AINode：异常检测、预测、分类）**；
  - 需要 **端到端 IoT 解决方案**（采集 + 存储 + 分析）；
- **典型用户**：工业 IoT 平台商、能源（电力、油气）、轨道交通、智能制造、智慧城市、车联网、运营商、农业物联网；
- **核心口号**：**"Apache IoTDB: Manage the IoT World"** + **"Designed for IoT"** + **"Edge-Cloud Collaboration"** + **"Time Series for Industry"**。

## 二、发展历史与社区

- **起源**：2016 年由 **清华大学软件学院黄向东团队** 启动研发；
- **里程碑**：
  - 2016：清华内部项目启动，**TsFile** 自研文件格式设计；
  - 2018-11：v0.8.x 首个内部版本发布；
  - 2019-08：**进入 Apache 孵化器**（Incubator）；
  - 2020-09：v0.11.x，引入 **多级存储（Tier Storage）+ 乱序写入优化**；
  - 2020-12：v0.12.x，引入 **Pipe 数据同步 + Trigger 触发器 + UDTF UDF**；
  - 2021-06：**晋升 Apache 顶级项目（TLP）**，**业界首个中国高校主导的 Apache TLP 数据库**；
  - 2021-12：v0.13.x，引入 **AINode（内置 AI 节点）+ Consensus 多协议**；
  - 2022-12：v1.0.0，**稳定版 GA**，引入 **ConfigNode / DataNode 分离 + Cluster 模式**；
  - 2023-09：v1.2.x，引入 **MQTT 协议接收 + OPC UA 客户端 + Modbus 桥接器**；
  - 2024-09：v1.3.x，引入 **QueryEngine 重构 + TsFile 4.0 + Pipe 双向同步**；
  - 2025：v1.4.x，引入 **AINode 增强（模型训练 + 部署）+ IoTDB-Workbench（工业 BI 工具）**；
  - 2026：v2.0.x（即将发布），**云原生重构 + K8s Operator GA + TsFile 5.0**；
- **维护状态**：极活跃，**每 3-4 个月一个 minor 版本**，GitHub ~5k stars（Apache 主仓），**300+ 贡献者**，**TsFile 单独 GitHub 仓库 ~1k stars**；
- **社区**：IoTDB Meetups（中国/美国/德国/印度/韩国）、IoTDB Conf 年度大会、Apache 邮件列表、Slack、Discord、清华大学 + 多家企业共同维护。

## 三、许可与商业限制

- **许可证**：**Apache 2.0**（完全开源，宽松）—— 这是 **TDengine（AGPL）vs IoTDB（Apache 2.0）** 的关键差异；
- **生态关系**：
  - **Apache 软件基金会顶级项目**（与 Apache Kafka、Apache Cassandra、Apache Flink 同级）；
  - **清华 + 时序数据库厂商生态**；
  - 与 **Grafana / Prometheus / EMQX / ThingsBoard** 集成；
  - 与 **Apache Kafka / Pulsar / RocketMQ** 双向 Pipe 同步；
  - 与 **Apache Flink / Spark / Beam** 通过 JDBC 集成；
  - 与 **Kubernetes**：官方 Helm Chart + IoTDB Operator（v2.0 GA）；
  - **TsFile 独立仓库**：`apache/tsfile` 可单独作为列存格式库使用。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **国家电网**：电力调度遥测（千亿级时间序列）；
  - **中石油**：油田 SCADA 数据；
  - **中国移动 / 联通 / 电信**：运营商网络监控；
  - **中航工业**：航空发动机遥测；
  - **中国商飞 C919**：飞行遥测；
  - **中车**：高铁运行监控；
  - **三一重工 / 徐工集团**：工程机械 IoT；
  - **海尔**：智能家电 + 工厂；
  - **海康威视**：视频监控 IoT 元数据；
  - **金风科技**：风电远程监控；
  - **隆基绿能**：光伏电站；
  - **海外**：CERN（欧洲核子研究组织）部分实验数据；
- **行业**：能源（电力、油气、新能源）、工业制造、轨道交通、航空航天、智慧城市、运营商、车联网、农业；
- **典型场景**：电力调度 SCADA、工业 PLC 数据采集、风电光伏远程监控、轨道交通车辆遥测、智慧工厂、智能水务、智能燃气。

## 五、一句话对比定位

> IoTDB 是 **"Apache 顶级 + TsFile 自研列存 + 端边云协同 + 工业协议原生 + AINode 集成 + Apache 2.0"** 的时序数据库；与 **TDengine（IoT 端到端 + AGPL）**、**InfluxDB（生态完整）**、**TimescaleDB（PG 兼容）**、**QuestDB（SQL 原生）**、**VictoriaMetrics（Prometheus 兼容）** 构成时序数据库阵营。IoTDB 的差异化是 **"Apache 顶级项目 + TsFile 列存 + 端边云协同 + 工业协议原生 + AINode + Apache 2.0 宽松"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────────────┐
│                     客户端 SDK / 协议层                                     │
│  - Java / C++ / Python / Go / Node.js / C# / REST                          │
│  - 原生 IoTDB Session API                                                   │
│  - JDBC / ODBC / Prometheus Remote Write                                   │
│  - MQTT v3.1.1 / v5.0（IoTDB 1.2+ 作为 Broker）                             │
│  - OPC UA Client（v1.2+，订阅外部 OPC UA）                                  │
│  - Modbus Bridge（v1.2+，桥接 Modbus 设备）                                │
│  - CAN / DDS / Profinet（生态合作伙伴）                                    │
│  - 边端：IoTDB Edge（资源 < 50MB ARM 平台）                                │
└─────────────────────┬──────────────────────────────────────────────────────┘
                      │ Session API / MQTT / OPC UA
┌─────────────────────▼──────────────────────────────────────────────────────┐
│                  IoTDB 集群（ConfigNode + DataNode + AINode）              │
│                                                                            │
│   ┌────────────────────────────────────────────────────────────────────┐ │
│   │  ConfigNode（管理节点）                                              │ │
│   │  ├─ 集群元数据：Region / DataPartition / SchemaPartition            │ │
│   │  ├─ Schema 管理：DB / Device / Measurement                        │ │
│   │  ├─ TTL 管理：数据保留策略                                          │ │
│   │  └─ Consensus：Ratis 2PC 协议                                      │ │
│   └────────────────────────────────────────────────────────────────────┘ │
│                                                                            │
│   ┌────────────────────────────────────────────────────────────────────┐ │
│   │  DataNode（数据节点，存储 + 查询）                                   │ │
│   │  ├─ StorageEngine：TsFile 存储                                     │ │
│   │  │   ├─ MemTable（写入缓冲，Aligned / Non-Aligned）                │ │
│   │  │   ├─ TsFile（不可变列存文件）                                    │ │
│   │  │   ├─ WAL（Write-Ahead Log）                                     │ │
│   │  │   └─ Tier Storage：Hot / Warm / Cold                          │ │
│   │  ├─ QueryEngine：查询算子 + Planner                                │ │
│   │  ├─ Pipe Engine：数据同步（IoTDB → IoTDB / Kafka / Pulsar）       │ │
│   │  ├─ Trigger Engine：实时触发器                                     │ │
│   │  └─ UDTF：用户自定义函数                                            │ │
│   └────────────────────────────────────────────────────────────────────┘ │
│                                                                            │
│   ┌────────────────────────────────────────────────────────────────────┐ │
│   │  AINode（AI 节点，v1.0+）                                          │ │
│   │  ├─ 内置时序 AI 模型：异常检测 / 预测 / 分类                       │ │
│   │  ├─ 模型训练 / 部署 / 调用                                          │ │
│   │  └─ ONNX / PMML 模型导入                                          │ │
│   └────────────────────────────────────────────────────────────────────┘ │
└─────────────────────┬──────────────────────────────────────────────────────┘
                      │
                      ▼
┌────────────────────────────────────────────────────────────────────────────┐
│   Tier Storage（分层存储）                                                  │
│   ├─ Hot：本地 SSD（最近 7 天）                                             │
│   ├─ Warm：本地 HDD（7-90 天）                                             │
│   └─ Cold：对象存储 S3 / OSS / HDFS（90+ 天）                              │
└────────────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **TsFile 自研列存格式**：区别于通用列存（Parquet / ORC），针对 IoT 时序优化（**设备级 + 时间级 + 列级** 三维索引）；
- **Device-Physical Model**：**每设备一条记录**，字段对齐（**Aligned Timeseries**），**IoT 场景查询加速 10x**；
- **双层引擎**：StorageEngine（写入 + 存储）+ QueryEngine（查询 + 分析），**Storage 与 Query 解耦**；
- **三层节点**：ConfigNode + DataNode + AINode，**职责清晰**，**AIOps 内置**；
- **Pipe 替代 ETL**：内置数据同步，**无需 Kafka / Flink**；
- **端边云协同**：Edge（资源极低）+ Standard（云端），**自动数据汇聚**；
- **工业协议**：MQTT Broker / OPC UA / Modbus 内置，**工业采集零代码**。

## 七、TsFile 列存文件格式

### 7.1 文件结构

```
TsFile
  ├── Magic "TsFile"
  ├── Version（v1 / v2 / v3 / v4 / v5）
  ├── ChunkGroup × N（每个 ChunkGroup = 一个设备的所有序列）
  │   ├── Device ID（设备路径，如 root.factory.d1）
  │   └── Chunk × M（每序列一个 Chunk）
  │       ├── Chunk Header：measurement + type + encoding + compression
  │       ├── Page × K（按时间区间）
  │       │   ├── Page Header：min/max ts + value count
  │       │   ├── Encoded Values
  │       │   └── Page Footer（CRC）
  │       └── Chunk Footer
  ├── Index Tree：设备级 / 测量级 / 时间级三层索引
  └── Footer（设备元数据 + 索引偏移）
```

### 7.2 关键特性

- **Aligned Chunk**（IoTDB 0.13+）：同一设备的多个时间序列存储为一个 ChunkGroup，**对齐写入 + 对齐压缩**，磁盘 I/O 降低 50%+；
- **三层索引**：
  - **设备级索引**：`Device ID → ChunkGroup 偏移`；
  - **测量级索引**：`Measurement ID → Chunk 偏移`；
  - **时间级索引**：`Time Range → Page 偏移`；
  - **查询 O(log N) 定位**；
- **自适应编码**（Per Series）：
  - **TS_2DIFF**（Delta-of-Delta，适合单调递增时间戳）；
  - **GORILLA**（XOR + LZ，适合 Float64）；
  - **RLE**（Run-Length，适合低基数）；
  - **PLAIN**（无压缩，适合高基数）；
  - **DICTIONARY**（字典编码，适合低基数字符串）；
  - **ZIGZAG + SIMPLE8B**（整数压缩）；
  - **LZ4 / ZSTD / SNAPPY**（通用压缩）；
- **Page 大小**（默认 1024 点 / Page）：平衡压缩率 + 读取粒度。

源码：IoTDB 主仓的 `iotdb-core/tsfile/` 模块 + 独立仓库 `apache/tsfile`。

## 八、存储引擎与乱序写入

### 8.1 MemTable

- **MemTable 抽象**：
  - 内存 HashMap（Device + Measurement → TVList）；
  - TVList：每序列时间序列值列表（**Aligned** 与 **Non-Aligned** 两类）；
  - 满阈值（默认 1024 Point）触发 flush；
- **乱序写入处理**（IoTDB **核心优势**）：
  - **主动乱序支持**：写入任意 timestamp 的数据，无需先排序；
  - **MemTable 内排序**：插入即按时间排序；
  - **Flush 后乱序数据合并**：TsFile 之间乱序数据**通过 Merge 自动归位**；
  - **乱序合并**：v0.13+ 引入 **UnsequenceTsFile**，与 SequenceTsFile 独立写入，**后台 Compaction 时合并**；
- **优势**：
  - 边端数据采集无需先排序；
  - 容忍网络延迟 / 设备时钟漂移；
  - 数据完整性更好（无丢数风险）。

### 8.2 TsFile 写入路径

```
Session API 写入
  → DataNode.StorageEngine
    ├→ Schema 校验（ConfigNode 缓存）
    ├→ WAL（追加写）
    ├→ MemTable 写入
    │   ├→ NonAligned：HashMap<Device, HashMap<Measurement, TVList>>
    │   └→ Aligned：HashMap<Device, AlignedTVList>
    ├→ 当 MemTable 满：
    │   ├→ Flush MemTable → TsFile（Sequence 或 Unsequence）
    │   └→ WAL truncate
    └→ 后台 Compaction：
        ├→ Sequence + Unsequence 合并
        ├→ 小 TsFile → 大 TsFile
        └→ 过期数据删除
```

### 8.3 读取路径

```
SQL → QueryEngine
  → 时间范围 + Device ID + Measurement
  → TsFile Index 定位
  → Chunk 读取 → Page 解码 → Filter 算子
  → Join / Group By / Window
  → 流式返回
```

## 九、AINode（AI 集成节点）

### 9.1 能力

- **内置时序模型**：
  - **异常检测**（Anomaly Detection）：基于 IForest / 深度学习；
  - **预测**（Forecasting）：基于 Transformer / LSTM / N-BEATS；
  - **分类**（Classification）：基于时序特征工程；
- **模型管理**：
  - 模型注册（`CREATE MODEL`）；
  - 模型训练（基于设备历史数据）；
  - 模型部署；
  - 模型调用（`CALL INFERENCE`）。

### 9.2 集成

```sql
-- 创建模型
CREATE MODEL anomaly_detector OPTIONS(
  MODEL_TYPE='IFOREST',
  TRAINING_DATA_SQL='SELECT value FROM root.factory.d1.sensor1 WHERE time > now() - 30d'
);

-- 调用
SELECT anomaly_detector(*) FROM root.factory.d1.sensor1;
```

源码：`iotdb-core/ainode/`。

## 十、Pipe 数据同步

### 10.1 能力

- **双向数据同步**：IoTDB ↔ IoTDB（同集群 / 跨集群 / 跨版本）；
- **多协议出口**：
  - IoTDB Sink（目标 IoTDB）；
  - Kafka Sink（v1.3+，写入 Kafka Topic）；
  - Pulsar Sink（v1.3+，写入 Pulsar Topic）；
  - File Sink（写入本地文件，便于离线分析）；
  - MySQL / PostgreSQL Sink（关系数据库同步）；
  - **OPC UA Sink**（工业总线协议）；
- **自定义 Processor**：
  - 内置：ChangeLog / Compression / Deduplication；
  - 自研：基于 PipeProcessor SPI；

### 10.2 配置示例

```sql
CREATE PIPE factory_to_cloud
WITH SOURCE ('path'='root.factory.**'),
WITH SINK ('sink'='iotdb-thrift-sink', 'node-urls'='cloud-server:6667');
```

源码：`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/pipe/`。

## 十一、Trigger 触发器与 UDTF

### 11.1 Trigger（实时触发器）

```sql
CREATE TRIGGER high_temp_alert
BEFORE INSERT ON root.factory.d1.temperature
AS 'com.example.HighTempTrigger';
```

- **类型**：`BEFORE INSERT` / `AFTER INSERT`；
- **场景**：实时告警、数据清洗、字段填充；
- **SPI**：`org.apache.iotdb.db.trigger.api.Trigger`；

### 11.2 UDTF（用户自定义函数）

```sql
CREATE FUNCTION my_udtf AS 'com.example.MyUDTF';

SELECT my_udsf(value) FROM root.factory.d1.sensor1;
```

- **类型**：UDTF（User Defined Time-Series Generating Function）；
- **场景**：复杂数据变换、特征提取；

源码：`iotdb-api/udf-api/`。

## 十二、降采样与连续查询

### 12.1 Continuous Query（CQ）

```sql
CREATE CONTINUOUS QUERY "cq_5m_avg"
BEGIN
  SELECT AVG(usage_idle) INTO root.factory.downsampled.d1_cpu_5m
  FROM root.factory.d1.cpu
  GROUP BY(5m)
END;
```

- **触发器**：ConfigNode 调度，每 5m 跑一次；
- **资源开销**：CQ SQL 解析 + 聚合执行；
- **限制**：CQ 不能跨集群（用 Pipe 替代）；

### 12.2 Tier Storage（分层存储）

```sql
SET CONFIG 'tier_strategy'='Hot,Warm,Cold';
SET CONFIG 'hot_to_warm_threshold'='604800000';  -- 7 天
SET CONFIG 'warm_to_cold_threshold'='7776000000';  -- 90 天
SET CONFIG 'cold_storage_path'='s3://bucket/path';
```

- **Hot**：本地 SSD（最近 7 天）；
- **Warm**：本地 HDD（7-90 天）；
- **Cold**：S3 / OSS / HDFS（90+ 天）；
- **数据迁移**：后台定时任务；
- **查询路由**：自动根据时间区间选 Tier；

### 12.3 保留策略

```sql
SET CONFIG 'data_region_group_size'='50000';
ALTER DATABASE root KEEP 3650;  -- 保留 10 年
```

- **TTL 策略**：过期 TsFile 整文件删除；
- **物理删除**：后台 TaskConfigManager 删除 TsFile + WAL；
- **备份恢复**：`iotdb-cli` + `TsFile Load / Dump` 工具。

## 十三、查询接口层

### 13.1 SQL（IoTDB SQL）

```sql
-- 标准 SQL + IoTDB 扩展
SELECT temperature, humidity
FROM root.factory.d1
WHERE time > 2026-01-01 AND time < 2026-02-01
  AND temperature > 80.0
ALIGN BY DEVICE;  -- IoTDB 特色：按设备对齐输出
```

- **特色语法**：
  - `ALIGN BY DEVICE`：按设备对齐输出（**vs 标准 SQL 按列对齐**）；
  - **IoTDB 路径语法**：`root.factory.d1.sensor1`（类似文件系统路径）；
  - `GROUP BY(5m)`：时间桶聚合；
  - `FILL` 语法；
  - `LATEST` 子句；
  - **窗口函数**（v1.0+）。

### 13.2 协议

- **IoTDB Session API**（Java/C++/Python/Go/Node.js/C#）；
- **JDBC / ODBC**；
- **Prometheus Remote Write**；
- **MQTT v3.1.1 / v5.0**（IoTDB 1.2+ 作为 Broker）；
- **REST API**；
- **Thrift / gRPC**（内部节点通信）；

## 十四、事务与一致性

- **写一致性**：
  - 单节点：**Atomic Per Row**（行级原子）；
  - 集群：**Ratis 2PC**（ConfigNode 元数据）+ **Consensus 协议**（DataNode 数据副本）；
- **读一致性**：默认 **Linearizable**（走 Consensus leader）；
- **隔离级别**：**Read Committed**；
- **乱序写入**：**完整支持**，主动设计；
- **数据完整性**：`CRC + Page Footer`，**无丢页风险**；
- **恢复**：WAL + 启动时自动 replay；

## 十五、复制与高可用

- **集群架构**：ConfigNode + DataNode（多副本），**Consensus 协议**（v1.0+）；
- **副本数**：默认 3（可配 2-5）；
- **数据均衡**：自动 Rebalance；
- **Pipe 跨集群**：异地容灾 + 跨集群数据汇聚；
- **AINode HA**：模型多副本部署；
- **MQTT Broker 内置**：IoTDB 1.2+ 自带 MQTT Broker，**工业协议入口无需外部组件**。

## 十六、运维与生态

- **监控**：`/metrics` Prometheus 端点；
- **Web UI**：`IoTDB Workbench`（v1.4+ 工业 BI 工具）；
- **Cli**：`iotdb-cli`（交互式 + 批处理）；
- **Kubernetes Operator**：官方 Operator（v2.0 GA）+ Helm Chart；
- **备份恢复**：
  - `LOAD TSFILE` 命令：物理导入 TsFile；
  - `DUMP TSFILE` 命令：物理导出 TsFile；
  - **Pipe**：跨集群同步即备份；
- **客户端 SDK**：
  - Java：`org.apache.iotdb.session.Session`；
  - Python：`apache-iotdb`（pip）；
  - Go：`apache/iotdb-client-go`；
  - C++ / Node.js / C#：官方包；
- **生态集成**：Grafana、Prometheus、EMQX、ThingsBoard、Apache Kafka、Flink、Spark、Hive、Superset。

## 十七、性能与基准

- **官方 benchmark**（`apache/iotdb-benchmark`）：
  - 写入：**百万级 points/sec**（单节点 + SSD）；
  - 查询：**亚秒级**响应（含乱序 + 大范围）；
- **TsFile vs Parquet**：
  - 写入：TsFile 更快（专为时序优化）；
  - 压缩：TsFile 自适应编码，压缩率与 Parquet 对齐；
  - 查询：TsFile 时间索引 + 设备索引更精细；
- **vs InfluxDB**：在 **乱序写入 + 工业协议** 场景 IoTDB 领先；
- **vs TimescaleDB**：在 **设备数 + 端边云** IoTDB 领先，**SQL 完整度** TimescaleDB 强；
- **vs TDengine**：在 **Apache 生态 + 乱序处理 + 工业协议** IoTDB 强，**流计算 + 多级存储** TDengine 强；
- **vs QuestDB**：在 **设备元数据 + 工业协议** IoTDB 强，**SQL 标准** QuestDB 强；
- **第三方基准**：Benchmarking Time Series Databases for IoT（VLDB 2020）、IoTDB TSBS 测试收录。

## 十八、学习路径

### 18.1 入门（1-2 周）

1. **官方文档**：`iotdb.apache.org/zh/UserGuide/`（中文为主 + 英文）；
2. **跑通 Docker**：`docker run -d -p 6667:6667 apache/iotdb:latest`；
3. **Cli 工具**：`./sbin/start-cli.sh` → SQL 操作；
4. **Java SDK**：`org.apache.iotdb.session.Session`；
5. **Grafana 接入**：官方 IoTDB 数据源。

### 18.2 进阶（4-6 周）

1. **TsFile 源码**（**必读**）：
   - `iotdb-core/tsfile/`：TsFile 主仓库代码；
   - `TsFileWriter`：TsFile 写入；
   - `TsFileSequenceReader`：TsFile 读取；
   - `ChunkGroupWriter` / `PageWriter`：Chunk 与 Page 编码；
   - `Encoding`（GORILLA / TS_2DIFF / RLE / DICTIONARY）；
   - **独立仓库 `apache/tsfile`**：作为通用列存格式库使用；
2. **StorageEngine 源码**：
   - `iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/`；
   - `MemTable`：内存缓冲；
   - `WAL`：写前日志；
   - `Compaction`：TsFile 后台合并；
   - **乱序合并**（UnsequenceTsFile + SequenceTsFile 合并）；
3. **QueryEngine 源码**：
   - `iotdb-core/datanode/src/main/java/org/apache/iotdb/db/queryengine/`；
   - `Plan`：执行计划；
   - `Executor`：算子执行；
   - **ALIGN BY DEVICE** 路径；
4. **Cluster 源码**：
   - `iotdb-core/confignode/`：ConfigNode；
   - `iotdb-core/datanode/src/main/java/org/apache/iotdb/db/confignode/`；
   - **Consensus 协议**（`iotdb-core/consensus/`）；

### 18.3 高级（长期）

1. **AINode**（`iotdb-core/ainode/`）：AI 模型集成；
2. **Pipe Engine**（`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/pipe/`）：数据同步；
3. **Trigger / UDTF**（`iotdb-api/trigger-api/`、`iotdb-api/udf-api/`）；
4. **MQTT Broker 集成**；
5. **TsFile 独立仓库**（作为通用列存格式）；
6. **边缘计算**（IoTDB Edge）；

### 18.4 推荐资源

- **官方**：IoTDB 官方文档（中文 + 英文）、Apache 邮件列表、官方博客；
- **论文**：
  - *"Apache IoTDB: A Time Series Database for Industrial IoT"*（黄向东等，SIGMOD 2022 Demo）；
  - *"TsFile: A Unified Columnar File Format for Industrial Time Series"*（VLDB 2021 Demo）；
  - *"Benchmarking Time Series Databases for IoT"*（Liu et al. 2020）；
- **书籍**：*《IoTDB 原理与实践》*（黄向东等，2023）；
- **视频**：IoTDB Conf、ApacheCon CFP、DTCC 中国数据库大会；
- **社区**：Apache 邮件列表（user@iotdb.apache.org）、GitHub Discussions、Slack、知乎 / CSDN。

## 十九、风险与陷阱

1. **ALIGN BY DEVICE 语法壁垒**：IoTDB 默认按设备对齐（与 SQL 按列对齐不同），**SQL 用户学习成本高**；
2. **Schema 强约束**：设备路径必须预先定义 schema，**动态设备路径支持较弱**（需 `SchemaTemplate` 解锁）；
3. **AINode 实验性**：内置 AI 模型对外部 AI 生态（PyTorch / ONNX）兼容有限，**复杂模型自研成本高**；
4. **Pipe 与 CQ 资源**：Pipe 实时同步占用 DataNode 资源，**大集群需独立 PipeWorker**；
5. **MQTT Broker 限制**：IoTDB 内置 MQTT Broker 仅基本功能，**不支持高级特性**（Bridge 到外部 Broker）；
6. **Cluster 模式运维复杂**：ConfigNode + DataNode + AINode 多节点运维比单节点要谨慎，**升级顺序要求严格**；
7. **生态绑定**：与 Apache 生态强绑定，**非 Java 系统集成需通过 JDBC 或 REST**；
8. **文档中文为主**：官方文档中文版最全，**英文版部分章节滞后**，国际团队参考需注意；
9. **冷数据查询延迟**：Cold Tier（S3 / OSS）查询延迟 100ms-1s，**关键监控需保留 Hot Tier 数据**；
10. **AINode 资源开销**：AINode 单独节点占用内存 / GPU 资源，**小集群部署成本不可忽视**；
11. **TsFile 压缩局限**：TsFile 对**非结构化数据**（文本、日志）压缩率不高，**仅适合时序数据**；
12. **工业协议版本演进**：OPC UA / Modbus / MQTT 协议版本众多，**IoTDB Bridge 兼容性需逐项验证**；
13. **生态成熟度（vs InfluxDB）**：InfluxDB 生态（Telegraf 300+ 插件）成熟度领先 IoTDB，**IoTDB 工业场景外社区生态较弱**。

## 二十、与本项目的关联

本项目 `engineering/` 与 IoTDB 的**潜在借鉴点**：

- **TsFile 自研列存格式**：`engineering/src/db/storage/columnar.c` 可参考 TsFile 的 ChunkGroup + Chunk + Page 三层结构；
- **乱序写入处理**：`engineering/src/db/storage/memtable.c` 可参考 IoTDB 的 **SequenceTsFile + UnsequenceTsFile 双轨设计**；
- **设备-物理模型**：`engineering/src/db/catalog/` 可参考 IoTDB 的 Device ID + Measurement 路径式 schema；
- **Pipe 数据同步**：`engineering/src/db/pipe/` 可参考 IoTDB 的 PipeProcessor 插件机制；
- **AINode 集成**：`engineering/apps/` 下若做时序 AI 集成，可参考 AINode 的 `CALL INFERENCE` 设计；
- **工业协议接收器**：`engineering/src/db/protocol/mqtt.c` 可参考 IoTDB 的 MQTT Broker 内置设计；
- **Apache 生态集成**：`engineering/src/db/integration/` 可参考 IoTDB 与 Kafka / Pulsar 的 Pipe 集成。

## 二十一、TSFile 编码与压缩：算法、复杂度与类型映射

§七 已给出 TsFile 的三层结构（ChunkGroup / Chunk / Page）与编码清单，本节下钻两层：**单个编码器内部的位打包算法**，以及 IoTDB 在 2.x 上「哪类序列默认走哪种编码」的**可配置映射**。

### 21.1 先定位代码：tsfile 已外置为独立 Maven 模块

自 IoTDB 2.x 起，TsFile 的编解码实现已从主仓剥离为独立模块 `org.apache.tsfile:tsfile`，版本在根 `pom.xml` 中单独锁定：

- `pom.xml:38` — `<version>2.0.11-SNAPSHOT</version>`（主仓版本）
- `pom.xml:149` — `<tsfile.version>2.4.1-260806-SNAPSHOT</tsfile.version>`
- `iotdb-core/datanode/pom.xml:59` — `<artifactId>tsfile</artifactId>`（DataNode 对它的依赖声明）

**这是一个研究 IoTDB 时极易踩的坑**：`TsFileIOWriter`、`TsFileSequenceReader`、`ChunkGroupWriter` 三个类在 2.0.11 主仓**全仓零命中**，因为它们的源码已随 tsfile 一起移出。`iotdb-core/` 下也没有 `node-ql`、更没有 `datanode-legacy` 这两个模块，只有 `ainode` / `antlr` / `calc-commons` / `confignode` / `consensus` / `datanode` / `metrics` / `node-commons` / `relational-grammar`。

因此**主仓看调度，tsfile 仓看编解码**。DataNode 侧保留的只是编排逻辑——何时 `startChunkGroup()`、何时切 Chunk、用哪个编码写 Page，例如 `iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/flush/MemTableFlushTask.java:350` 的 `this.writer.startChunkGroup(deviceId)` 与同文件 `MemTableFlushTask.java:353` 的 `endChunkGroup()` 分支。

### 21.2 类型 → 编码的默认映射（自适应编码的落地形态）

「自适应」不是运行时探测，而是**建序列时按数据类型绑定默认编码**，之后可按序列覆盖。默认表在 `iotdb-core/datanode/src/main/java/org/apache/iotdb/db/conf/IoTDBConfig.java`：

| 数据类型 | 默认编码 | 源码行 | 选择依据 |
|---------|---------|--------|---------|
| BOOLEAN | `TSEncoding.RLE` | `IoTDBConfig.java:678` | 布尔只有 2 种取值，RLE 退化成 1 bit/点 |
| INT32 | `TSEncoding.TS_2DIFF` | `IoTDBConfig.java:681` | 计数器/状态码近似等差，二阶差分近全 0 |
| INT64 | `TSEncoding.TS_2DIFF` | `IoTDBConfig.java:684` | 同上，且时间戳列同样走 TS_2DIFF |
| FLOAT | `TSEncoding.GORILLA` | `IoTDBConfig.java:687` | 浮点低有效位熵低，XOR 后前导零多 |
| DOUBLE | `TSEncoding.GORILLA` | `IoTDBConfig.java:690` | 同上 |
| TEXT | `TSEncoding.PLAIN` | `IoTDBConfig.java:693` | 字符串无结构可利用，直存最省 CPU |

值得注意：**INT64 与 TIMESTAMP 都用 TS_2DIFF**。等间隔采集（1Hz / 100kHz 工业采样）下时间戳列几乎全部退化为常量 0，这是 TsFile 最大的单点收益来源。

`GORILLA_V1`、`CHIMP`、`ZIGZAG`、`SIMPLE8B`、`LOSSY_DS` 这几个名字在 2.0.11 主仓**零命中**，属于 tsfile 仓内部的枚举值或历史名，引用前必须回 tsfile 仓核对。可验证的补充项是 `DICTIONARY`——测试工具 `iotdb-core/datanode/src/test/java/org/apache/iotdb/db/storageengine/dataregion/compaction/utils/TsFileGeneratorUtils.java:314` 用 `TSEncoding.DICTIONARY` 构造 TEXT 列；而 `PLAIN` / `PLAIN_DICTIONARY` / `RLE` / `REGULAR` 在 JDBC 驱动 `iotdb-client/jdbc/src/main/java/org/apache/iotdb/jdbc/IoTDBDatabaseMetadata.java:149`~`IoTDBDatabaseMetadata.java:155` 以 SQL 保留字形式暴露，说明它们是 `SET ENCODING` 语法层可用的名字。

### 21.3 TS_2DIFF：二阶差分

核心恒等式是差分的分配律：

```
secondDiff_i = (v_i - v_{i-1}) - (v_{i-1} - v_{i-2})
             = Δ_i - Δ_{i-1}
             = v_i - 2·v_{i-1} + v_{i-2}
```

```text
编码（单遍流式，state = (firstValue, secondValue)）:
  firstValue  ← v[0];  firstValueBytes ← bits(firstValue)
  secondValue ← v[1]; secondValueBytes ← bits(secondValue)
  输出 firstValueBytes, secondValueBytes
  writeVarInt(secondValue - firstValue)          // Δ_1
  writeVarInt((secondValue - firstValue) - 0)    // Δ_1 - Δ_0，Δ_0 视作 0
  for i = 2 .. n-1:
      value   ← v[i]
      current ← value - secondValue
      if current == 0:   write(0)                 // 1 bit 命中，等间隔采样的快路径
      else:               write(1); writeVarInt(current - (secondValue - firstValue))
      firstValue  ← secondValue
      secondValue ← value

解码（镜像状态机）:
  for i = 2 .. n-1:
      current ← prevValueDelta + Δ_{i-2}
      value   ← current + prevValue
```

**位成本分析**。设 `w = bits(secondDiff_i)`，尾数用 varint（`⌈log2|w|⌉ + 1` 位）编码，控制位 1 位：

| 场景 | 每点成本 |
|------|---------|
| 严格等间隔（secondDiff ≡ 0） | 1 bit（只有控制位） |
| 抖动采样（secondDiff ∈ {−1,0,+1}） | 1 + 2 = 3 bit |
| 一般等差递增（Δ 固定） | 1 bit |
| 随机整数（Δ 本身随机） | 1 + O(log range)，**比 PLAIN 更差** |

关键结论：**TS_2DIFF 的收益完全来自「Δ 的平稳性」，而不是二阶差分本身**。对随机整数它会退化到与 PLAIN 同量级甚至更差，因此 Int32 随机序列应覆盖为 PLAIN。对比 PLAIN 的固定 `32` 或 `64` bit/点，平稳整数列可获得 **32×** 量级压缩比。

复杂度：编码/解码均 **O(n) 时间、O(1) 额外空间**（流式，非块式）。

### 21.4 GORILLA：异或 + 前导零打包

适用于 FLOAT / DOUBLE。核心是把相邻值的 IEEE-754 位模式做异或：

```
xor_i = bits(v_i) ⊕ bits(v_{i-1})
```

两个相近浮点数共享高位（符号 + 指数 + 有效数高位），因此 `xor_i` 的**前导零（leading zeros）与尾部零（trailing zeros）都很长**，只存有效位。

```text
state = (previousBits, storedLeadingZeros = -1)

写入第一个值:  writeBits(bits(v[0]), 64)

for i = 1 .. n-1:
    currentBits ← bits(v[i])
    xor ← currentBits ⊕ previousBits
    if xor == 0:                          write(0)              // 完全相同，最快路径
    else:
        leading  ← bitLength(64) - bitLength(xor)   // 前导零
        trailing ← bitLength(xor & -xor) - 1       // 尾部零
        if storedLeadingZeros ∈ {-1, leading} and 1 ≤ trailing ≤ 6:
              write(1); writeBits(trailing, 2)                // 仅尾部零变化
        else:
              write(1); writeBits(leading, 5)
              if leading ≥ 31:  write(1); writeBits(64 - leading - 1, 6)
              else:            write(0); writeBits(leading, 5)
              if trailing == 0: write(0)
              else:            write(1); writeBits(trailing, 6)
              writeBits(xor >>> trailing, bitLength(xor) - trailing)
        storedLeadingZeros ← leading
    previousBits ← currentBits
```

**位成本分析**。前导零用 5 位定长（上限 31），首值之后的值最少 1 位控制位 + 5 位前导零 = 6 bit；若尾部零恰为 1~6 且前导零未变，只需 **1 + 2 = 3 bit**。

| 场景 | 每点成本 |
|------|---------|
| 传感器读数连续不变 | 1 bit |
| 缓慢漂移（前导零稳定、尾部零 1~6） | 3 bit |
| 常规抖动 | 1 + 5 + 5 + n（n = 有效位数，通常 8~20） |
| 对比 PLAIN DOUBLE | 64 bit |

即典型工业曲线可从 **64 bit/点** 压到 **15~25 bit/点**，约 **3~4×**。代价是 GORILLA **不支持随机访问**：必须顺序解码到第 i 点，因此 TsFile 只能以 Page 为单位解压，这正是 Page 粒度存在的根本原因（见 §二十二）。

复杂度：**O(n) 时间、O(1) 状态**；解码是编码的严格逆运算。

### 21.5 RLE 与 PLAIN：两条基线

**RLE** 编码「连续相同值的个数 + 该值」，控制位 1 bit：

```text
若 v[i] != v[i-1]:  write(0); writeBits(bits(v[i]), w)      // 变点，w 位原值
否则:              write(1); writeVarInt(runLength - 1)      // 重复
```

BOOLEAN（`IoTDBConfig.java:678`）走这条路时 `w = 1`，恒定序列可达 **1 bit/点**；但**交替序列（0101…）反而劣于 PLAIN**（1 + varint(0) = 2 bit vs 1 bit）。IoTDB 用 PLAIN 存 BOOLEAN 时必须逐列覆盖，这是 §19「Schema 强约束」之外另一个易踩的配置坑。

**PLAIN** 无变换、直存 `w` 位/bit。看似最差，但它是**唯一 O(1) 随机访问**的编码，且当输入本就不规则时与 TS_2DIFF 打平。对 TEXT（`IoTDBConfig.java:693`）它是唯一合理选择。

### 21.6 压缩器族：通用压缩 + `LOSS` 有损通道

编码之后还有一层通用压缩。**WAL 自身的压缩**是可验证的配置项：`IoTDBConfig.java:1234` — `private CompressionType WALCompressionAlgorithm = CompressionType.LZ4;`（枚举全名为 `CompressionType`，来自 `org.apache.tsfile.file.metadata.enums`，**不是** `CompressionCodec`）。

通用压缩器的可选值可从测试工具反推——`TsFileGeneratorUtils.java:330`~`TsFileGeneratorUtils.java:336` 依次使用 `UNCOMPRESSED` / `SNAPPY` / `GZIP` / `LZ4` / `ZSTD`。

**有损压缩走 `LOSS` 属性通道**，同一通道下有两个取值：`deadband`（死区）与 `sdt`。`AnalyzeVisitor.java:2661`~`AnalyzeVisitor.java:2662` 把 `deadband` 直接重命名为 `loss`，即两者共用一条属性校验路径；`IoTDBConstant.java:236` 把 `DEADBAND` / `LOSS` / `SDT` 与三个 SDT 参数一并列入 `ALLOWED_SCHEMA_PROPS` 白名单。SDT 的参数定义在 `IoTDBConstant.java:222`~`IoTDBConstant.java:226`：

```java
public static final String LOSS            = "loss";
public static final String SDT             = "sdt";
public static final String SDT_COMP_DEV    = "compdev";     // 允许的最大偏差
public static final String SDT_COMP_MIN_TIME = "compmintime"; // 最短压缩时长（未达则跳过）
public static final String SDT_COMP_MAX_TIME = "compmaxtime"; // 最长压缩时长（达到则必压）
```

校验逻辑在 `iotdb-core/datanode/src/main/java/org/apache/iotdb/db/schemaengine/schemaregion/utils/MetaFormatUtils.java`：

- `MetaFormatUtils.java:114` `checkTimeseriesProps` —— 仅当 `props[LOSS] == "sdt"` 时才进入 SDT 校验（即 SDT 是**逐序列 opt-in**）；
- `MetaFormatUtils.java:124` 必须提供 `compdev`，缺失即抛 `IllegalParameterOfPathException`；
- `MetaFormatUtils.java:130` 解析 `compdev` 为 `double`，负值抛异常（`MetaFormatUtils.java:132`），格式错误抛异常（`MetaFormatUtils.java:135`）；
- `MetaFormatUtils.java:140` / `MetaFormatUtils.java:141` 分别解析 `compmintime` 与 `compmaxtime`；
- `MetaFormatUtils.java:145` 要求 `compMaxTime > compMinTime`。

从这组参数可以反推 SDT 的**误差有界契约**：`|近似值 - 原值| ≤ compdev`，且「最短/最长压缩时长」共同约束了压缩比与延迟的取舍窗口。SDT 的具体变换算法实现位于外置 tsfile 模块，**主仓内无法证实**——网上流传的「Fourier + Golomb-Rice」说法在本仓无任何代码依据，勿直接引用。

同理，`PLA`（被解读为 Dremel 的 Pattern Length Array 压缩器）在本仓**仅作为 SQL 保留字**出现在 `IoTDBDatabaseMetadata.java:155`，与压缩无关；`LOSSY_DS` 全仓零命中。这两个名字应从 IoTDB 的压缩器清单中剔除。

## 二十二、MemTable 刷盘流水线与分层 Compaction

§8.1/§8.2 描述了 MemTable 与写入路径的概念形态，本节给出**三段式刷盘流水线的真实结构**与**Compaction 的候选文件判定算法**。

### 22.1 MemTable 的两级映射与 Aligned 双形态

MemTable 的物理结构是 `Map<IDeviceID, IWritableMemChunkGroup>`（`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/memtable/AbstractMemTable.java:88`），两级懒创建：

- **非对齐（tree model 普通序列）**：`AbstractMemTable.java:165` `createMemChunkGroupIfNotExistAndGet` → 建 `WritableMemChunkGroup`，`seriesNumber` 加 1（`AbstractMemTable.java:175`）；
- **对齐（Aligned Timeseries）**：`AbstractMemTable.java:181` `createAlignedMemChunkGroupIfNotExistAndGet` → 建 `AlignedWritableMemChunkGroup`，`seriesNumber` 按 `filteredSchemaList.size()` **批量**加（`AbstractMemTable.java:189`），随后 `AbstractMemTable.java:190` 返回。

实现类是 `PrimitiveMemTable`（`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/memtable/PrimitiveMemTable.java:27`），继承 `AbstractMemTable`。代价计数器只有三个：`memSize`（`AbstractMemTable.java:96`）、`seriesNumber`（`AbstractMemTable.java:104`）、`totalPointsNum`（`AbstractMemTable.java:106`），每个 insert 分支末尾统一 `memSize += …` 与 `totalPointsNum += pointsInserted`。

刷盘触发依赖 `tvListSortThreshold`（`IoTDBConfig.java:441` — 默认 `0`，即**每个序列在刷盘时都强制排序一次**，不做「乱序程度低就免排」的启发式优化）。

### 22.2 三段式刷盘流水线：排序 → 编码 → I/O

`MemTableFlushTask`（`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/flush/MemTableFlushTask.java:62`）不是单线程循环，而是**两个 `BlockingQueue` 串起的三阶段生产者-消费者管线**：

- `MemTableFlushTask.java:78` `encodingTaskQueue`（`LinkedBlockingQueue<Object>`）—— 承载排序后的待编码任务；
- `MemTableFlushTask.java:79` `ioTaskQueue` —— 承载编码后的待写盘字节。

```text
阶段 0（主线程，syncFlushMemTable，:131）:
    deviceIDList ← sort(lexicographic, memTableMap.keySet())      # :160 保证 ChunkGroup 落盘顺序稳定
    for deviceID in deviceIDList:
        if 该设备/ChunkGroup 为空: continue
        encodingTaskQueue.put(StartFlushGroupIOTask(deviceID))    # :167 交给 I/O 侧 startChunkGroup
        seriesInOrder ← sort(value.keySet())                        # :169 列序确定 → 决定 Chunk 布局
        for seriesId in seriesInOrder:
            series.sortTvListForFlush()                             # :179 排序 = TVList 的归并路径
            encodingTaskQueue.put(series)                           # :183
        encodingTaskQueue.put(EndChunkGroupIoTask())                # :186 交给 I/O 侧 endChunkGroup
    encodingTaskQueue.put(TaskEnd())                                # :188 毒丸，终止编码阶段
    await encodingTaskFuture; await ioTaskFuture

阶段 1（encodingTask 线程池）:
    while (task = encodingTaskQueue.take()) != TaskEnd:
        if task 是 StartFlushGroupIOTask / EndChunkGroupIoTask:
            ioTaskQueue.put(task)                                   # :258 分发控制信号
        else:
            task.encode(ioTaskQueue, encodeInfo, times)             # :279
            若 encodeInfo 达阈值 → 收口当前 Chunk 并 ioTaskQueue.put(TaskEnd)
    ioTaskQueue.put(TaskEnd())                                       # :286

阶段 2（ioTask 线程池）:
    while (msg = ioTaskQueue.take()) != TaskEnd:
        StartFlushGroupIOTask → writer.startChunkGroup(deviceId)     # :350
        EndChunkGroupIoTask   → writer.endChunkGroup()               # :353
        其余                    → writer.writePage(...)

收尾: writer.writePlanIndices()                                     # :211 写三层索引
```

这个设计的算法意义是**编码（CPU-bound）与 I/O（disk-bound）解耦**：满载时二者可并行，重叠区间的展开率提升接近 2×。`Collections.sort` 在阶段 0 用**字典序**而非哈希序（`MemTableFlushTask.java:160`、`MemTableFlushTask.java:169`），保证同一批设备多次刷盘产出**字节级一致的 ChunkGroup 布局**，这对 Compaction 的去重与 diff 有意义。

### 22.3 Chunk 切分的双阈值

编码过程中何时封口一个 Chunk，由 `BatchEncodeInfo`（`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/utils/datastructure/BatchEncodeInfo.java`）的三个阈值联合判定，构造点在 `MemTableFlushTask.java:112`~`MemTableFlushTask.java:121`：

```java
long MAX_NUMBER_OF_POINTS_IN_CHUNK = config.getTargetChunkPointNum();  // :112
long TARGET_CHUNK_SIZE              = config.getTargetChunkSize();      // :113
this.encodeInfo = new BatchEncodeInfo(0, 0, 0,
    MAX_NUMBER_OF_POINTS_IN_PAGE,          // MemTableFlushTask.java:119，来自 tsfile 库配置
    MAX_NUMBER_OF_POINTS_IN_CHUNK,         // MemTableFlushTask.java:120
    TARGET_CHUNK_SIZE);                    // MemTableFlushTask.java:121
```

默认值（`IoTDBConfig.java`）：

| 配置项 | 默认值 | 源码行 | 作用 |
|-------|-------|--------|------|
| `targetChunkSize` | `1600000L`（≈1.53 MB） | `IoTDBConfig.java:505` | **按字节**封顶，防单 Chunk 过大 |
| `targetChunkPointNum` | `100000L` | `IoTDBConfig.java:508` | **按点数**封顶，防窄序列 Chunk 过密 |

判定逻辑在 `iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/memtable/WritableMemChunk.java:495`：

```java
if (encodeInfo.pointNumInChunk >= encodeInfo.maxNumberOfPointsInChunk
    || encodeInfo.dataSizeInChunk >= encodeInfo.targetChunkSize) {   // WritableMemChunk.java:495-496 或关系
    // 收口当前 Chunk，写 ioTaskQueue，并 encodeInfo.resetPointAndSize()
}
```

两个阈值**取或**（先到先封），这是标准的「同时限制逻辑行数与物理字节」策略：点数阈值防止 1 字节/点 的稀疏列产生海量元数据，字节阈值防止 1000 字节/点 的 TEXT 列撑爆 Page。

`flushOneMemTable` 位于 `iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/memtable/TsFileProcessor.java:1832`：取 `flushingMemTables.getFirst()`，空 MemTable 直接跳过（`TsFileProcessor.java:1837`），否则 `writer.mark()` 后投递 `MemTableFlushTask` 并 `syncFlushMemTable()`；抛异常时进入 `writer.reset()` 截断损坏数据路径（`TsFileProcessor.java:1878`）。

### 22.4 WAL：先记日志、后改内存

WAL 写入与 MemTable 写入在同一临界区内，顺序固定为「写 WAL → 改 MemTable」。`WALWriter`（`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/wal/io/WALWriter.java:32`）继承 `LogWriter`，默认版本 `WALFileVersion.V3`（`WALWriter.java:38` / `WALWriter.java:41`），承载两条流：

- **数据流**：`WALWriter.java:54` `write(ByteBuffer buffer, WALMetaData metaData)` —— 先 `updateMetaData(metaData)` 累加元数据，再落 buffer；
- **元数据流**：收尾 `endFile()`（`WALWriter.java:69`）写三段尾巴：`WALSignalEntry(WAL_FILE_INFO_END_MARKER)`（`WALWriter.java:74`）→ `metaData.serialize`（`WALWriter.java:84`）→ 4 字节 `metaDataSize` → 版本 magic（`WALWriter.java:87`）。启动时反向扫描 magic 定位元数据区。

WAL 快照（`walMemTableSnapshotThreshold`，`IoTDBConfig.java:214` — 默认 `8MB`）触发进度快照，配合 `ProgressWALReader` 做增量恢复。

注意命名：2.x 的写入单元是 **`WALEntry`**（`WALEntryType.WAL_FILE_INFO_END_MARKER`），**不是** `WALRecord`——该类名在本仓零命中。

### 22.5 InnerSpace Compaction：以「层级」为一等公民

**层级（level）就编码在文件名里**。`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/tsfile/generator/TsFileNameGenerator.java:418`：

```java
private static final String FILE_NAME_PATTERN = "(\\d+)-(\\d+)-(\\d+)-(\\d+).tsfile$";
```

四段依次为 `{time}-{version}-{innerCompactionCnt}-{crossCompactionCnt}`（解析见 `TsFileNameGenerator.java:161`），其中 **`innerCompactionCnt`（`TsFileNameGenerator.java:441` `getInnerCompactionCnt()`）就是 Size-Tiered 的 level**：level 0 = 从未被压缩过的原始刷盘产物，每经过一次 inner compaction 加 1（`TsFileNameGenerator.java:207`）。

`SizeTieredCompactionSelector`（`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/compaction/selector/impl/SizeTieredCompactionSelector.java:59`）**从低层往高层搜，某层一旦命中就停止上探**（类注释 `SizeTieredCompactionSelector.java:50`~`SizeTieredCompactionSelector.java:57`）。核心循环 `selectTsFileResourcesByLevel`（`SizeTieredCompactionSelector.java:102`）：

```text
selectTsFileResourcesByLevel(level):
    target ← config.getTargetCompactionFileSize()    # 2 GiB，SizeTieredCompactionSelector.java:105
    fileLimit ← config.getInnerCompactionCandidateFileNum()   # 30，SizeTieredCompactionSelector.java:106
    for currentFile in tsFileResources:              # 资源列表按时间从旧到新
        if currentFile.getTsFileName().getInnerCompactionCnt() != level:
            # 遇到别的层 → 封口当前批（若 size > 1），重置，继续
            continue
        if cannotSelectCurrentFileToNormalCompaction(currentFile):   # SizeTieredCompactionSelector.java:121
            selected.clear(); continue                # 非 NORMAL 状态或待 repair → 清空重来
        totalSizeIfSelect ← selectedSize + currentFile.getTsFileSize()
        if totalSizeIfSelect > target or selected.size() >= fileLimit:   # SizeTieredCompactionSelector.java:129-130
            if selected.size() > 1: taskList.add(copy(selected))         # SizeTieredCompactionSelector.java:134
            selected ← [currentFile]; selectedSize ← currentFile.size()
        else:
            selected.add(currentFile); selectedSize ← totalSizeIfSelect
    if selected.size() == fileLimit: taskList.add(copy(selected))         # SizeTieredCompactionSelector.java:157 整除批直接提交
    if hasNextTimePartition and selected.size() > 1: taskList.add(...)   # SizeTieredCompactionSelector.java:165 跨时间分区强制提交
```

三个设计细节值得单独拎出来：

1. **`selected.size() > 1` 而非 `>= 2`**：单文件 compaction 是纯浪费 I/O 而无收益，因此恒被拒；
2. **`SizeTieredCompactionSelector.java:165` 的 `hasNextTimePartition` 逃生门**——若下一个时间分区尚无数据，本分区残留的少量文件必须提前合并，否则会永久滞留在小文件状态。这是 TTL + 分区写入下的关键收敛保证；
3. **`SizeTieredCompactionSelector.java:121` 的清空式重置**——遇到 `TsFileResourceStatus != NORMAL` 或待 repair 的文件直接丢弃整批，保证 compaction 链不与非 NORMAL 资源交叉。

默认阈值（`IoTDBConfig.java`）：`targetCompactionFileSize = 2147483648L`（2 GiB，`IoTDBConfig.java:502`）、`innerCompactionCandidateFileNum = 30`（`IoTDBConfig.java:532`）、`innerCompactionTotalFileNumThreshold = 100`（`IoTDBConfig.java:497`，分区文件总数熔断）、`innerCompactionTaskSelectionModsFileThreshold = 128 * 1024L`（128 KB，`IoTDBConfig.java:603`，改删记录过大时改走「先扫 mods 再选文件」的高成本路径）。

任务类型由 `iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/compaction/constant/CompactionTaskType.java:22` 的六值枚举界定：`INNER_SEQ` / `INNER_UNSEQ` / `CROSS` / `INSERTION` / `SETTLE` / `REPAIR`。**不存在** `SIZE_BASED` / `LEVEL_BASED` / `TIMESLICE_BASED` 这三个策略名——选择器是「按层级遍历 + 双阈值」，不是可切换的策略枚举。

### 22.6 CrossSpace Compaction：逐设备的区间重叠判定

Seq / Unseq 汇合的关键是**判定哪些 seq 文件与某个 unseq 文件真正重叠**。`CrossSpaceCompactionCandidate`（`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/compaction/selector/utils/CrossSpaceCompactionCandidate.java:39`）的 `prepareNextSplit`（`CrossSpaceCompactionCandidate.java:92`）对 unseq 文件里的**每个设备**逐个扫描 seq 文件列表：

```text
prepareNextSplit():
    unseqFile ← unseqFiles[nextUnseqFileIndex]
    if not unseqFile.hasDetailedDeviceInfo(): return false   # 已被 TTL 删除，无法升级索引
    for unseqDeviceInfo in unseqFile.getDeviceInfoIterator():
        atLeastOneSeqSelected ← false; previousSeqFile ← null
        for seqFile in seqFiles:
            if (not seqFile.isValidCandidate or not hasDetailedDeviceInfo)
               and seqFile.mayHaveOverlapWithUnseqFile(unseqDeviceInfo):
                return false            # 关键：可能重叠却无效 → 必须中止，不能跳过
            if not seqFile.containsDevice(deviceId): continue      # CrossSpaceCompactionCandidate.java:116
            seqDeviceInfo ← seqFile.getDeviceInfoById(deviceId)
            if seqFile.unsealed() and unseqDeviceInfo.endTime >= seqDeviceInfo.startTime:
                return false            # 未封口文件不能作为合并对象
            if unseqDeviceInfo.endTime <= seqDeviceInfo.endTime:
                tmpSplit.addSeqFileIfNotSelected(seqFile); seqFile.markAsSelected()   # CrossSpaceCompactionCandidate.java:130-131
                atLeastOneSeqSelected ← true; break
            else if unseqDeviceInfo.startTime <= seqDeviceInfo.endTime:
                tmpSplit.addSeqFileIfNotSelected(seqFile); seqFile.markAsSelected()   # CrossSpaceCompactionCandidate.java:135-136
                atLeastOneSeqSelected ← true
            else:
                if not seqFile.unsealed(): previousSeqFile ← seqFile   # 记录「恰好在前面」的那个
        # 兜底：TTL/Delete 删掉了本该匹配的文件时，把 previousSeqFile 拉进来
        if atLeastOneSeqSelected or previousSeqFile == null: continue
        if not previousSeqFile.isValidCandidate: return false
        tmpSplit.addSeqFileIfNotSelected(previousSeqFile)            # CrossSpaceCompactionCandidate.java:165
    unseqFile.markAsSelected(); return true
```

**判定谓词**就是标准区间重叠：`unseqDeviceInfo.startTime <= seqDeviceInfo.endTime && unseqDeviceInfo.endTime >= seqDeviceInfo.startTime`（对应 `CrossSpaceCompactionCandidate.java:126` 与 `CrossSpaceCompactionCandidate.java:134` 两个分支）。而 `CrossSpaceCompactionCandidate.java:165` 的 `previousSeqFile` 兜底处理的是两类边界场景——TTL 删除了目标 seq 文件、或 unseq 文件由 `LOAD` 操作导入，导致区间重叠检查在合并后失效。

复杂度：单次扫描 **O(|unseqDevices| × |seqFiles|)**，且 `markAsSelected` 保证同一 seq 文件不会跨 split 重复入选，因此**最坏情况线性**。实际由 `maxCrossCompactionFileNum` / `maxCrossCompactionFileSize`（`RewriteCrossSpaceCompactionSelector.java:77` / `RewriteCrossSpaceCompactionSelector.java:78` 两个字段）截断。

合并执行侧由 `iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/compaction/execute/performer/impl/FastCompactionPerformer.java` 承担：`FastCompactionPerformer.java:205` `compactionWriter.startChunkGroup(device, isAligned)` 开启设备组，`FastCompactionPerformer.java:213` `endChunkGroup()` 收口——可见 Compaction 输出与 §22.2 刷盘路径**共用同一套 ChunkGroup 语义**。

### 22.7 Settle 改写与 TTL 淘汰

删除/更新（`DELETE FROM` / `ALTER`）在 IoTDB 中不做原地修改，而是留下 **mods 记录**，等文件被「settle」重写时才真正生效。`SettleSelectorImpl`（`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/compaction/selector/impl/SettleSelectorImpl.java:59`）把候选分成 `fullyDirtyResources`（整文件作废）与 `partiallyDirtyResources`（部分脏），触发阈值复用了 inner compaction 的两个参数（`SettleSelectorImpl.java:148`）：

```java
return partiallyDirtyResources.size() >= config.getInnerCompactionCandidateFileNum()
    || totalPartiallyDirtyFileSize >= config.getTargetCompactionFileSize();
```

即「脏文件攒够 30 个或 2 GiB 就开一次 settle 重写」。方法名 `selectFileBaseOnModSize` / `selectFileBaseOnDirtyData`（`SettleSelectorImpl.java:178` / `SettleSelectorImpl.java:180`）对应 mods 大小阈值（128 KB）两侧的高/低成本选择路径。

**TTL 淘汰**是独立的定时任务 `TTLScheduleTask`（`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/dataregion/compaction/schedule/TTLScheduleTask.java:36`），`Callable<Void>` 形态：按 `ttlCheckInterval`（`TTLScheduleTask.java:43`，来自 `CommonDescriptor`）周期 `Thread.sleep`（`TTLScheduleTask.java:57`），醒来后对快照中的每个 DataRegion 调 `executeTTLCheck()`（`TTLScheduleTask.java:67`），被 `StopTTLCheckException` 或中断时静默退出。

## 二十三、查询算子链、四级元数据下推与 ChunkCache

§13 描述了 SQL 接口层与 `ALIGN BY DEVICE` 语法，本节下钻到**算子实现**与**元数据裁剪算法**。

### 23.1 算子链与 Volcano 迭代模型

典型查询 `SELECT temperature FROM root.f.d1 WHERE time > T0 AND temperature > 80 ORDER BY temperature DESC LIMIT 10` 的算子链：

```
SeriesScanOperator  →  AggregationOperator  →  TreeTopKOperator  →  Sink
（源算子）              （聚合）                （TopN）
```

- **源算子**：`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/queryengine/execution/operator/source/SeriesScanOperator.java:36`，继承 `AbstractSeriesScanOperator`，构造时把 `SeriesScanUtil` 装进来（`SeriesScanOperator.java:48`）。输出粒度受 Page 约束：`SeriesScanOperator.java:50` / `SeriesScanOperator.java:51` 把 `maxReturnSize` 取为 `min(maxReturnSize, TSFileDescriptor.getConfig().getPageSizeInByte())`，即**单次返回不超过一个 Page 的解码量**。Aligned 版本是同目录的 `AlignedSeriesScanOperator`，聚合版本为 `SeriesAggregationScanOperator` / `AlignedSeriesAggregationScanOperator`。
- **聚合算子**：`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/queryengine/execution/operator/process/AggregationOperator.java:47`，继承 `AbstractConsumeAllOperator`（**消费型**，需把 input 全部读空）。主循环 `calculateNextAggregationResult`（同文件 `AggregationOperator.java:155`）对每个 `TreeAggregator` 调 `processTsBlocks`，然后逐个 `inputTsBlocks[i].skipFirst()` 前进游标（`AggregationOperator.java:164`），空了就置 `null` 释放引用（`AggregationOperator.java:167`）。
- **TopN 算子**：`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/queryengine/execution/operator/process/TreeTopKOperator.java:35`，只是 `TopKOperator`（`iotdb-core/calc-commons/src/main/java/org/apache/iotdb/calc/execution/operator/process/TopKOperator.java:56`）的树模型薄壳——共享实现放在 `calc-commons` 里供树模型与表模型复用。

### 23.2 四级元数据下推：文件 → 序列 → Chunk → Page

这是 IoTDB 查询剪枝的核心，也是「列存 + 三层索引」收益真正落地的地方。四级过滤谓词**完全相同**，只有元数据粒度不同：

```java
// iotdb-core/datanode/src/main/java/org/apache/iotdb/db/queryengine/execution/operator/source/SeriesScanUtil.java:2067
private boolean filterAllSatisfy(Filter filter, IMetadata metadata) {
  return filter == null || filter.allSatisfy(metadata);
}
```

`IMetadata` 是文件 / `ITimeSeriesMetadata` / `ChunkMetaData` / `IPageReader` 的共同父接口，因此**同一个 `Filter` 对象可以逐级复用**。下推点在 `SeriesScanUtil`：

| 层级 | 判定位置 | 剪掉什么 |
|------|---------|---------|
| 文件 | `SeriesScanUtil.java:321` 调 `filterFirstTimeSeriesMetadata()`（定义在 `SeriesScanUtil.java:1963`，判定在 `SeriesScanUtil.java:1990` / `SeriesScanUtil.java:1991`） | 时间区间与文件整体不相交的文件 |
| 序列 | `SeriesScanUtil.java:347` / `SeriesScanUtil.java:348` | `minTime/maxTime` 已排除该序列的 Chunk |
| Chunk | `SeriesScanUtil.java:438` 调 `filterFirstChunkMetadata()`（定义在 `SeriesScanUtil.java:443`，判定在 `SeriesScanUtil.java:469` / `SeriesScanUtil.java:470`） | 逐 Chunk 的 `Statistics` |
| Page | `SeriesScanUtil.java:871` / `SeriesScanUtil.java:872`（另有 `SeriesScanUtil.java:1501` / `SeriesScanUtil.java:1502` 的复用分支） | 逐 Page 解码前的最后拦截 |

两个 Filter 的语义分工（`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/queryengine/plan/planner/plan/parameter/SeriesScanOptions.java`）：

- `globalTimeFilter`（`SeriesScanOptions.java:43`）——**全局时间谓词**，在上述四级全部生效；
- `pushDownFilter`（`SeriesScanOptions.java:48`）——**值谓词**（`temperature > 80`），同样四级生效；
- `topKRuntimeFilter`（`SeriesScanOptions.java:59`）——见 23.4。

`SeriesScanUtil.java:152` 还把 `getMaxNumberOfPointsInPage()` 缓存为 `MAX_NUMBER_OF_POINTS_IN_PAGE`，说明 Page 是**遍历的基本计数单位**：查询的时间复杂度正比于「通过四级过滤的 Page 数」而非文件总点数。

### 23.3 TopN 的有界堆：O(N log K) 而非 O(N log N)

朴素 `ORDER BY … LIMIT K` 需要把全部 N 行物化再排序。IoTDB 用**容量为 K 的有界大顶堆**替代，`iotdb-core/calc-commons/src/main/java/org/apache/iotdb/calc/utils/datastructure/MergeSortHeap.java`：

```java
// TopKOperator.java:121 —— 反转比较器，把 top-K 变成「保留 K 个最大值」
this.mergeSortHeap = new MergeSortHeap(topValue, comparator.reversed());
```

堆操作（`MergeSortHeap`）：

```java
// MergeSortHeap.java:41
public void push(MergeSortKey mergeSortKey) {
  if (heapSize == 0) heap[0] = mergeSortKey;
  shiftUp(heapSize, mergeSortKey);
  ++heapSize;
}
// MergeSortHeap.java:49
public MergeSortKey poll() {
  MergeSortKey res = heap[0];
  heap[0] = heap[heapSize - 1];
  shiftDown(0, heap[0]);
  heapSize--;
  return res;
}
```

主循环里的关键优化是**「堆顶即第 K 名」剪枝**：

```text
for each row r:
    if heap.size < K:  heap.push(r)
    elif r better than heap.top:  heap.poll(); heap.push(r)      # 堆顶出 → r 入
    else:                         丢弃 r                            # 恒定时间，O(1)
```

复杂度：**O(N log K) 时间、O(K) 空间**。当 `K = 10`（典型 `LIMIT`）而 `N = 10^8` 时，空间从 ~1 GB 降到 ~KB 级，且每行的比较代价从 O(log N) 降到 O(1)（仅与堆顶比一次）。相比全排序的 O(N log N)，这是 **TopN 场景下最有效的单点优化**。

代价是结果**无序**——堆给出的是 top-K 集合而非有序流，故 `TreeTopKOperator` 之后仍需一次 K 元素排序，代价 O(K log K)，可忽略。

### 23.4 TopK 运行时过滤：把 LIMIT 反推成时间范围

`TopKRuntimeFilter`（`iotdb-core/calc-commons/src/main/java/org/apache/iotdb/calc/execution/filter/TopKRuntimeFilter.java:30`）把「按 value 排序取 top-K」翻译成「value 的 min/max 阈值」，用于在 23.2 的四级下推里直接砍掉不可能满足阈值的 Chunk：

- `TopKRuntimeFilter.java:54` `mayQualify(long time)` —— 单时间点判定；
- `TopKRuntimeFilter.java:59` `mayQualifyRange(long startTime, long endTime)` —— 区间判定，利用 `Statistics` 的时间范围与当前已见 min/max 比对。

接入点：`SeriesScanUtil.java:355` 的 `mayQualifyRuntimeFilterRange(Statistics)`，当 `scanOptions.getTopKRuntimeFilter() == null` 时直接返回 `true`（`SeriesScanUtil.java:357` / `SeriesScanUtil.java:358`，即无 TopN 时零开销），否则委托 `filter.mayQualifyRange(statistics.getStartTime(), statistics.getEndTime())`（`SeriesScanUtil.java:360`）。

它与 23.3 的有界堆是**互补**关系：堆解决「读进来的 N 行怎么少排序」，`TopKRuntimeFilter` 解决「N 行里有多少能不看」。前者省 CPU，后者省 I/O——只有后者能在 TsFile 解码前生效。

### 23.5 可增量合并的聚合：Welford 方差

聚合必须可分解（partial → final），否则无法在多 DataRegion / 多线程间并行。COUNT/SUM 天然可加，AVG 需带 count，**方差**最难——直接累加 `Σx²` 会有严重的数值抵消（catastrophic cancellation）。

`iotdb-core/calc-commons/src/main/java/org/apache/iotdb/calc/execution/aggregation/VarianceAccumulator.java` 用 **Welford 在线算法**维护三元组 `(count, mean, m2)`（`VarianceAccumulator.java:49` / `VarianceAccumulator.java:50` / `VarianceAccumulator.java:51`），合并两个部分结果（`VarianceAccumulator.java:101`~`VarianceAccumulator.java:107`）：

```java
long   newCount = count + intermediateCount;
double newMean  = ((intermediateCount * intermediateMean) + (count * mean)) / newCount;
double delta    = intermediateMean - mean;
m2 = m2 + intermediateM2 + delta * delta * intermediateCount * count / newCount;
count = newCount;
mean  = newMean;
```

这是 Chan-Golub-LeVeque 的并行 Welford 合并公式。若方差类型是样本方差，最终再除以 `count - 1`。滑动窗口的**反向**合并（`VarianceAccumulator.java:122`~`VarianceAccumulator.java:128`）对称地做减法：

```java
long   newCount = count - intermediateCount;
double newMean  = ((count * mean) - (intermediateCount * intermediateMean)) / newCount;
double delta    = intermediateMean - mean;
m2 = m2 - intermediateM2 - delta * delta * intermediateCount * count / newCount;
```

同一套三元组同时服务 `AggregationOperator`（多输入流合并）、`SeriesAggregationScanOperator`（按 `Statistics` 预聚合，见 `Accumulator.addStatistics`）与 `SlidingWindowAggregationOperator`（滑窗增删）。**全程 O(1) 状态、O(1) 合并**，且数值稳定性显著优于 `E[x²] - E[x]²`。

### 23.6 ChunkCache：按字节计权的 LRU

`iotdb-core/datanode/src/main/java/org/apache/iotdb/db/storageengine/buffer/ChunkCache.java:61` 缓存**解压后的 Chunk**（不是字节流），策略是 Caffeine 支撑的 **LRU**（类注释 `ChunkCache.java:58`）。构造器（`ChunkCache.java:85`~`ChunkCache.java:98`）：

```java
lruCache = Caffeine.newBuilder()
    .maximumWeight(CACHE_MEMORY_BLOCK.getTotalMemorySizeInBytes())    // ChunkCache.java:92 上限是内存块的字节数
    .weigher((Weigher<ChunkCacheKey, Chunk>)
        (key, chunk) -> (int)(key.getRetainedSizeInBytes()
                            + chunk.getRetainedSizeInBytes()))       // ChunkCache.java:94-96 权重 = key + chunk 的驻留字节
    .recordStats()                                                   // ChunkCache.java:97 打开统计
    .build();
```

三个设计要点：

1. **`maximumWeight` 而非 `maximumSize`**——容量单位是**字节**，`weigher` 同时计入 key 与 value 的驻留大小。因为时序 Chunk 大小跨越几个数量级（1 点 vs 100 万点），按「条目数」限流会剧烈抖动，按字节限流才稳定。
2. **`ChunkCacheKey`（`ChunkCache.java:228`）刻意不存 measurementId**——源码注释明说「to save memory footprint, we don't save measurementId in ChunkHeader of Chunk」，即测量名不进缓存 key，改为在 `Chunk` 内恢复。对高频小 Chunk 场景，key 的常驻开销本身不可忽略。
3. **命中率可观测**：`ChunkCache.java:104` / `ChunkCache.java:105` 的 `getHitRate()` 返回 `lruCache.stats().hitRate() * 100`；`ChunkCache.java:202` 另有原始 `hitRate()`、`ChunkCache.java:206` `evictionCount()`、`ChunkCache.java:214` `averageLoadPenalty()`。当 `averageLoadPenalty` 显著大于 0 时，说明 cache 容量不足、线程在 eviction 上被阻塞——这是调 `ChunkCache` 内存块的直接信号。

**复杂度**：`get` 平均 **O(1)**；命中为 0 次 I/O，未命中为一次 Chunk 解码（因 GORILLA/TS_2DIFF 不可随机访问，解码量等于整个 Chunk 的点数）。因此缓存命中率直接决定 CPU 开销：

```text
期望解码点数 ≈ (1 - hitRate) × chunkPoints
平均 I/O 次数 ≈ (1 - hitRate) × chunkCount
```

这与 23.2 的四级下推是**乘法关系**：下推决定「多少 Chunk 被访问」，缓存决定「其中多少免于解码」。

## 二十四、参考资料

1. IoTDB 官方文档（中文）：<https://iotdb.apache.org/zh/UserGuide/>
2. IoTDB 官方文档（英文）：<https://iotdb.apache.org/UserGuide/>
3. IoTDB GitHub：<https://github.com/apache/iotdb>
4. TsFile GitHub：<https://github.com/apache/tsfile>
5. IoTDB SIGMOD 2022 Demo：<https://dl.acm.org/doi/10.1145/3538712.3538737>
6. Apache IoTDB 主仓：<https://iotdb.apache.org/
7. IoTDB 工作机制：<https://iotdb.apache.org/zh/SystemDesign/1_TsFile/1_TsFile.html>
8. IoTDB 论文合集：<https://iotdb.apache.org/Publication.html
9. IoTDB 性能测试：<https://github.com/apache/iotdb-benchmark
10. IoTDB vs InfluxDB 对比：<https://iotdb.apache.org/zh/UserGuide/Ecosystem-System/Comparison/Comparison-with-Other-Databases.html