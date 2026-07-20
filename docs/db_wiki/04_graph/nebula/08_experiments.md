# NebulaGraph 动手实验

## 环境准备

```bash
# Docker Compose 部署 NebulaGraph
docker network create nebula-net

# 创建 docker-compose.yml
cat > docker-compose.yml << 'EOF'
version: '3'
services:
  metad0:
    image: vesoft/nebula-metad:v3.6
    ...
  storaged0:
    image: vesoft/nebula-storaged:v3.6
    depends_on: [metad0]
    ...
  graphd0:
    image: vesoft/nebula-graphd:v3.6
    depends_on: [metad0]
    ports: ["9669:9669"]
EOF

docker-compose up -d

# 连接 Console
docker run --rm -it --network nebula-net \
  vesoft/nebula-console:v3.6 \
  -addr graphd0 -port 9669
```

## 实验 1：创建图空间和 Schema

```ngql
-- 创建图空间
CREATE SPACE IF NOT EXISTS social(
  vid_type=FIXED_STRING(30),
  partition_num=15,
  replica_factor=1
);

-- 使用图空间
USE social;

-- 创建标签
CREATE TAG IF NOT EXISTS person(name string, age int);

-- 创建边类型
CREATE EDGE IF NOT EXISTS knows(likeness int);

-- 查看 Schema
SHOW TAGS;
SHOW EDGES;
```

## 实验 2：插入和查询

```ngql
-- 插入顶点
INSERT VERTEX person(name, age) VALUES
  "Alice": ("Alice", 30),
  "Bob": ("Bob", 25),
  "Carol": ("Carol", 28);

-- 插入边
INSERT EDGE knows(likeness) VALUES
  "Alice"->"Bob": (90),
  "Bob"->"Carol": (85),
  "Alice"->"Carol": (75);

-- 查询
FETCH PROP ON person "Alice" YIELD properties(vertex);

-- 图遍历
GO FROM "Alice" OVER knows YIELD dst(edge) AS friend;
```

## 实验 3：索引和 LOOKUP

```ngql
-- 创建索引
CREATE TAG INDEX IF NOT EXISTS idx_person_name ON person(name);

-- 重建索引
REBUILD TAG INDEX idx_person_name;

-- 使用索引查询
LOOKUP ON person WHERE person.name == "Alice" YIELD person.name, person.age;
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| 创建图空间 | social 图空间 | |
| 插入顶点 | 3 个 person | |
| 插入边 | 3 条 knows | |
| 图遍历 | Bob, Carol | |
| LOOKUP 索引 | 使用索引加速 | |

## 要点总结

- nGQL 类 SQL，易学
- Schema 需要先定义再使用
- LOOKUP 利用索引加速
- GO 支持多跳遍历