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

## 二十一、参考资料

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