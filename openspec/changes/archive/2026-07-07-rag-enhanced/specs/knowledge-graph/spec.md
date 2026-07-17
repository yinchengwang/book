# 知识图谱规格

> 状态: 草稿 | 创建日期: 2026-07-07

## 1. 概述

### 1.1 目的

定义知识图谱（Knowledge Graph）的数据结构和操作接口，用于 Graph RAG 中的实体管理和关系查询。

### 1.2 核心概念

```
┌─────────────────────────────────────────────────────────────────────┐
│                         知识图谱结构                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│                        ┌─────────────┐                              │
│                        │  KNOWLEDGE  │                              │
│                        │    GRAPH    │                              │
│                        └──────┬──────┘                              │
│                               │                                      │
│          ┌────────────────────┼────────────────────┐                │
│          │                    │                    │                │
│          ▼                    ▼                    ▼                │
│   ┌─────────────┐     ┌─────────────┐     ┌─────────────┐         │
│   │  ENTITIES   │     │  RELATIONS  │     │  ADJACENCY  │         │
│   │             │     │             │     │             │         │
│   │ ┌─────────┐ │     │ ┌─────────┐ │     │ ┌─────────┐ │         │
│   │ │Person:A │ │     │ │  WORKS   │ │     │ │ A → [B] │ │         │
│   │ │  Entity │ │     │ │  FOR     │ │     │ │ B → [C] │ │         │
│   │ └─────────┘ │     │ └────┬────┘ │     │ │ C → []  │ │         │
│   │ ┌─────────┐ │     │      │      │     │ └─────────┘ │         │
│   │ │  Org:B  │ │◀────┼──────┘      │     │             │         │
│   │ │  Entity │ │     │             │     │             │         │
│   │ └─────────┘ │     └─────────────┘     └─────────────┘         │
│   │      │                                                │         │
│   │      │ (degree)                                       │         │
│   └──────┼────────────────────────────────────────────────┘         │
│          │                                                         │
│          ▼                                                         │
│   ┌─────────────────────────────────────────────────────────────┐  │
│   │  Vertex A ──▶ WORKS_FOR ──▶ Vertex B ──▶ LOCATED_IN ──▶ C  │  │
│   └─────────────────────────────────────────────────────────────┘  │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## 2. 数据结构

### 2.1 KGEntity (实体)

```cpp
namespace rag {

/**
 * @brief 知识图谱实体
 */
struct KGEntity {
    std::string id;                    // 唯一标识: hash(name + type)
    std::string name;                  // 实体名称
    std::string type;                  // 实体类型
    
    // 描述和属性
    std::string description;           // LLM 生成的描述
    std::unordered_map<std::string, std::string> properties;
    
    // 来源追踪
    std::string source_chunk_id;       // 来源 Chunk ID
    float confidence = 1.0f;           // 置信度 [0, 1]
    
    // 向量表示
    std::vector<float> embedding;      // 实体向量
    
    // 统计
    int degree = 0;                    // 连接数（出度 + 入度）
    
    // 元数据
    int64_t created_at = 0;            // 创建时间戳
    int64_t updated_at = 0;            // 更新时间戳
};

/**
 * @brief 实体类型枚举
 */
enum class EntityType {
    PERSON,       // 人物
    ORG,          // 组织/机构
    LOC,          // 地点
    CONCEPT,      // 概念
    TECHNOLOGY,   // 技术
    PRODUCT,      // 产品
    EVENT,        // 事件
    UNKNOWN       // 未知
};

/**
 * @brief 实体类型工具函数
 */
inline const char* entity_type_to_string(EntityType type) {
    switch (type) {
        case EntityType::PERSON: return "PERSON";
        case EntityType::ORG: return "ORG";
        case EntityType::LOC: return "LOC";
        case EntityType::CONCEPT: return "CONCEPT";
        case EntityType::TECHNOLOGY: return "TECHNOLOGY";
        case EntityType::PRODUCT: return "PRODUCT";
        case EntityType::EVENT: return "EVENT";
        default: return "UNKNOWN";
    }
}

inline EntityType string_to_entity_type(const std::string& type) {
    if (type == "PERSON") return EntityType::PERSON;
    if (type == "ORG") return EntityType::ORG;
    if (type == "LOC") return EntityType::LOC;
    if (type == "CONCEPT") return EntityType::CONCEPT;
    if (type == "TECHNOLOGY") return EntityType::TECHNOLOGY;
    if (type == "PRODUCT") return EntityType::PRODUCT;
    if (type == "EVENT") return EntityType::EVENT;
    return EntityType::UNKNOWN;
}

}  // namespace rag
```

### 2.2 KGRelation (关系)

```cpp
namespace rag {

/**
 * @brief 知识图谱关系
 */
struct KGRelation {
    std::string id;                    // 唯一标识: hash(source + type + target)
    std::string source_id;            // 源实体 ID
    std::string target_id;            // 目标实体 ID
    std::string type;                  // 关系类型
    
    // 关系详情
    std::string description;          // LLM 生成的关系描述
    float weight = 1.0f;              // 关系权重
    float confidence = 1.0f;          // 置信度 [0, 1]
    
    // 来源追踪
    std::string source_chunk_id;      // 来源 Chunk ID
    std::string context;              // 上下文文本
    
    // 反向关系
    std::string reverse_type;         // 反向关系类型
};

/**
 * @brief 关系类型枚举
 */
enum class RelationType {
    WORKS_FOR,      // 为...工作
    LOCATED_IN,     // 位于
    USES,           // 使用
    PART_OF,        // 属于
    RELATED_TO,     // 相关于
    DEPENDS_ON,     // 依赖于
    IMPLEMENTS,     // 实现
    CREATED_BY,     // 由...创建
    KNOWS,          // 认识
    ALLIES_WITH,    // 与...结盟
    UNKNOWN
};

}  // namespace rag
```

### 2.3 关系类型映射

| 关系类型 | 反向类型 | 示例 |
|----------|----------|------|
| WORKS_FOR | HAS_EMPLOYEE | 张三 → WORKS_FOR → 百度 |
| LOCATED_IN | CONTAINS | 北京 → LOCATED_IN → 中国 |
| USES | USED_BY | Python → USES → 机器学习 |
| PART_OF | HAS_PART | 部门 → PART_OF → 公司 |
| RELATED_TO | RELATED_TO | RAG → RELATED_TO → LLM |
| DEPENDS_ON | REQUIRED_FOR | React → DEPENDS_ON → Node.js |

## 3. 接口规范

### 3.1 KnowledgeGraph 类

```cpp
namespace rag {

/**
 * @brief 知识图谱接口
 */
class KnowledgeGraph {
public:
    virtual ~KnowledgeGraph() = default;
    
    // ========== 实体操作 ==========
    
    /**
     * @brief 添加实体
     * @param entity 实体对象
     * @return 是否成功
     */
    virtual bool add_entity(const KGEntity& entity) = 0;
    
    /**
     * @brief 更新实体
     * @param entity 实体对象
     * @return 是否成功
     */
    virtual bool update_entity(const KGEntity& entity) = 0;
    
    /**
     * @brief 删除实体
     * @param entity_id 实体 ID
     * @return 是否成功
     */
    virtual bool remove_entity(const std::string& entity_id) = 0;
    
    /**
     * @brief 获取实体
     * @param entity_id 实体 ID
     * @return 实体对象，如果不存在返回 nullopt
     */
    virtual std::optional<KGEntity> get_entity(
        const std::string& entity_id) const = 0;
    
    /**
     * @brief 按类型获取实体
     * @param type 实体类型
     * @return 实体列表
     */
    virtual std::vector<KGEntity> get_entities_by_type(
        const std::string& type) const = 0;
    
    /**
     * @brief 搜索实体
     * @param query 查询文本
     * @param top_k 返回数量
     * @return 相似实体列表
     */
    virtual std::vector<KGEntity> search_entities(
        const std::string& query, int top_k) = 0;
    
    // ========== 关系操作 ==========
    
    /**
     * @brief 添加关系
     * @param relation 关系对象
     * @return 是否成功
     */
    virtual bool add_relation(const KGRelation& relation) = 0;
    
    /**
     * @brief 删除关系
     * @param relation_id 关系 ID
     * @return 是否成功
     */
    virtual bool remove_relation(const std::string& relation_id) = 0;
    
    /**
     * @brief 获取关系
     * @param relation_id 关系 ID
     * @return 关系对象
     */
    virtual std::optional<KGRelation> get_relation(
        const std::string& relation_id) const = 0;
    
    /**
     * @brief 获取实体的所有关系
     * @param entity_id 实体 ID
     * @return 关系列表
     */
    virtual std::vector<KGRelation> get_relations(
        const std::string& entity_id) const = 0;
    
    // ========== 图遍历 ==========
    
    /**
     * @brief 获取邻居实体
     * @param entity_id 实体 ID
     * @param depth 深度（跳数）
     * @return 邻居实体 ID 列表
     */
    virtual std::vector<std::string> get_neighbors(
        const std::string& entity_id, int depth = 1) = 0;
    
    /**
     * @brief 获取两点间的路径
     * @param from_id 起始实体 ID
     * @param to_id 目标实体 ID
     * @param max_depth 最大深度
     * @return 路径列表
     */
    virtual std::vector<std::vector<std::string>> get_paths(
        const std::string& from_id,
        const std::string& to_id,
        int max_depth = 3) = 0;
    
    // ========== 统计 ==========
    
    virtual size_t entity_count() const = 0;
    virtual size_t relation_count() const = 0;
    
    // ========== 持久化 ==========
    
    /**
     * @brief 保存图谱到文件
     * @param path 文件路径
     */
    virtual void save(const std::string& path) = 0;
    
    /**
     * @brief 从文件加载图谱
     * @param path 文件路径
     */
    virtual void load(const std::string& path) = 0;
    
    /**
     * @brief 清空图谱
     */
    virtual void clear() = 0;
};

}  // namespace rag
```

## 4. 行为规范

### 4.1 实体操作

| 操作 | 行为 |
|------|------|
| 添加已存在实体 | 更新现有实体，合并属性 |
| 删除实体 | 同时删除相关关系 |
| 获取不存在实体 | 返回 nullopt |
| 重复添加相同实体 | 去重，不创建重复 |

### 4.2 关系操作

| 操作 | 行为 |
|------|------|
| 添加关系 | 自动增加相关实体 degree |
| 删除关系 | 自动减少相关实体 degree |
| 添加已存在关系 | 更新现有关系 |
| 关系端点不存在 | 自动创建端点实体 |

### 4.3 图遍历

| 场景 | 行为 |
|------|------|
| depth = 0 | 返回空列表 |
| depth = 1 | 返回直接邻居 |
| depth = N | 返回 N 跳内所有邻居 |
| 循环检测 | 避免重复访问 |
| 不存在的实体 | 返回空列表 |

## 5. 图遍历算法

### 5.1 BFS 邻居查询

```cpp
std::vector<std::string> KnowledgeGraph::get_neighbors(
    const std::string& entity_id, int depth) {
    
    std::vector<std::string> neighbors;
    std::unordered_set<std::string> visited;
    std::queue<std::pair<std::string, int>> queue;
    
    queue.emplace(entity_id, 0);
    visited.insert(entity_id);
    
    while (!queue.empty()) {
        auto [current, current_depth] = queue.front();
        queue.pop();
        
        if (current_depth >= depth) continue;
        
        // 获取邻居
        auto relations = get_relations(current);
        for (const auto& rel : relations) {
            std::string neighbor_id;
            if (rel.source_id == current) {
                neighbor_id = rel.target_id;
            } else {
                neighbor_id = rel.source_id;
            }
            
            if (visited.insert(neighbor_id).second) {
                if (current_depth + 1 > 0) {  // 不包含起点
                    neighbors.push_back(neighbor_id);
                }
                queue.emplace(neighbor_id, current_depth + 1);
            }
        }
    }
    
    return neighbors;
}
```

### 5.2 路径查找

```cpp
std::vector<std::vector<std::string>> KnowledgeGraph::get_paths(
    const std::string& from_id,
    const std::string& to_id,
    int max_depth) {
    
    std::vector<std::vector<std::string>> paths;
    std::vector<std::string> current_path = {from_id};
    
    std::function<void(const std::string&, int)> dfs = 
        [&](const std::string& current, int depth) {
            if (depth > max_depth) return;
            
            if (current == to_id) {
                paths.push_back(current_path);
                return;
            }
            
            auto relations = get_relations(current);
            for (const auto& rel : relations) {
                std::string next_id = (rel.source_id == current) 
                    ? rel.target_id : rel.source_id;
                
                if (std::find(current_path.begin(), current_path.end(), next_id) 
                    == current_path.end()) {
                    current_path.push_back(next_id);
                    dfs(next_id, depth + 1);
                    current_path.pop_back();
                }
            }
        };
    
    dfs(from_id, 0);
    return paths;
}
```

## 6. 存储格式

### 6.1 JSON 格式

```json
{
  "version": "1.0",
  "entities": [
    {
      "id": "ent_123456",
      "name": "RAG",
      "type": "CONCEPT",
      "description": "Retrieval Augmented Generation",
      "properties": {
        "full_name": "Retrieval Augmented Generation",
        "field": "NLP"
      },
      "source_chunk_id": "chunk_001",
      "confidence": 0.95,
      "degree": 3,
      "created_at": 1720000000,
      "updated_at": 1720000000
    }
  ],
  "relations": [
    {
      "id": "rel_789",
      "source_id": "ent_123456",
      "target_id": "ent_654321",
      "type": "RELATED_TO",
      "description": "RAG uses LLM",
      "weight": 1.0,
      "confidence": 0.9,
      "source_chunk_id": "chunk_001"
    }
  ]
}
```

## 7. 与 graph_engine 集成

### 7.1 GraphEngineStorage 实现

```cpp
class GraphEngineStorage : public KnowledgeGraph {
public:
    GraphEngineStorage(const std::string& data_dir);
    ~GraphEngineStorage();
    
    bool add_entity(const KGEntity& entity) override;
    bool update_entity(const KGEntity& entity) override;
    bool remove_entity(const std::string& entity_id) override;
    std::optional<KGEntity> get_entity(const std::string& entity_id) const override;
    
    bool add_relation(const KGRelation& relation) override;
    std::vector<KGRelation> get_relations(const std::string& entity_id) const override;
    
    std::vector<std::string> get_neighbors(const std::string& entity_id, int depth) override;
    
    void save(const std::string& path) override;
    void load(const std::string& path) override;

private:
    // ID 映射
    std::unordered_map<std::string, graph_vertex_id_t> entity_to_vertex_;
    std::unordered_map<graph_vertex_id_t, std::string> vertex_to_entity_;
    
    // 图句柄
    void* graph_;
};
```

### 7.2 存储映射

| KG 概念 | graph_engine 概念 |
|---------|-------------------|
| Entity | Vertex |
| Relation | Edge |
| entity_id | vertex_id |
| relation_id | edge_id |
| get_neighbors(depth) | BFS traversal |

## 8. 配置规范

### 8.1 配置文件

```yaml
knowledge_graph:
  # 存储路径
  data_dir: "./rag_data/knowledge_graph"
  
  # 持久化格式: json | binary
  storage_format: "json"
  
  # 实体类型
  entity_types:
    - PERSON
    - ORG
    - LOC
    - CONCEPT
    - TECHNOLOGY
    - PRODUCT
  
  # 关系类型
  relation_types:
    - WORKS_FOR
    - LOCATED_IN
    - USES
    - PART_OF
    - RELATED_TO
  
  # 质量控制
  min_confidence: 0.5
  max_entities_per_chunk: 50
  max_relations_per_chunk: 30
  
  # 图遍历
  max_traversal_depth: 3
  max_paths: 10
```

## 9. 性能规范

### 9.1 操作延迟

| 操作 | 延迟 |
|------|------|
| add_entity | < 1ms |
| get_entity | < 0.1ms |
| get_neighbors(depth=1) | < 5ms |
| get_neighbors(depth=2) | < 20ms |
| search_entities | < 50ms |

### 9.2 容量限制

| 项目 | 限制 |
|------|------|
| 实体数量 | 100,000 |
| 关系数量 | 1,000,000 |
| 每个实体属性 | 100 |
| 图遍历深度 | 10 |

## 10. 测试规范

### 10.1 单元测试

```cpp
TEST(KnowledgeGraph, AddAndGetEntity) {
    GraphEngineStorage kg("./test_kg");
    
    KGEntity entity;
    entity.id = "test_001";
    entity.name = "Test Entity";
    entity.type = "CONCEPT";
    entity.description = "A test entity";
    
    EXPECT_TRUE(kg.add_entity(entity));
    
    auto retrieved = kg.get_entity("test_001");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "Test Entity");
}

TEST(KnowledgeGraph, GetNeighbors) {
    GraphEngineStorage kg("./test_kg");
    
    // 添加实体和关系
    kg.add_entity({"id": "A", "name": "A", "type": "CONCEPT"});
    kg.add_entity({"id": "B", "name": "B", "type": "CONCEPT"});
    kg.add_entity({"id": "C", "name": "C", "type": "CONCEPT"});
    
    kg.add_relation({"id": "r1", "source_id": "A", "target_id": "B", "type": "RELATED_TO"});
    kg.add_relation({"id": "r2", "source_id": "B", "target_id": "C", "type": "RELATED_TO"});
    
    // 测试 1 跳邻居
    auto neighbors_1 = kg.get_neighbors("A", 1);
    ASSERT_EQ(neighbors_1.size(), 1);
    EXPECT_EQ(neighbors_1[0], "B");
    
    // 测试 2 跳邻居
    auto neighbors_2 = kg.get_neighbors("A", 2);
    ASSERT_EQ(neighbors_2.size(), 2);
    EXPECT_TRUE(std::find(neighbors_2.begin(), neighbors_2.end(), "B") != neighbors_2.end());
    EXPECT_TRUE(std::find(neighbors_2.begin(), neighbors_2.end(), "C") != neighbors_2.end());
}

TEST(KnowledgeGraph, RemoveEntityCascadesRelations) {
    GraphEngineStorage kg("./test_kg");
    
    kg.add_entity({"id": "X", "name": "X", "type": "CONCEPT"});
    kg.add_entity({"id": "Y", "name": "Y", "type": "CONCEPT"});
    kg.add_relation({"id": "r", "source_id": "X", "target_id": "Y", "type": "RELATED_TO"});
    
    kg.remove_entity("X");
    
    EXPECT_FALSE(kg.get_entity("X").has_value());
    EXPECT_TRUE(kg.get_relations("Y").empty());
}
```
