/**
 * @file test_kg.cpp
 * @brief 知识图谱单元测试
 */

#include <gtest/gtest.h>
#include "rag/knowledge_graph.h"
#include "rag/entity_extractor.h"

using namespace rag;

// ========== 测试夹具 ==========

class KnowledgeGraphTest : public ::testing::Test {
protected:
    void SetUp() override {
        kg_ = create_knowledge_graph();
    }

    std::unique_ptr<KnowledgeGraph> kg_;
};

// ========== 实体 CRUD 测试 ==========

TEST_F(KnowledgeGraphTest, AddAndGetEntity) {
    KGEntity entity;
    entity.id = "e1";
    entity.name = "测试实体";
    entity.type = EntityType::TECHNOLOGY;
    entity.description = "用于测试的实体";
    entity.confidence = 0.9f;

    // 添加实体
    EXPECT_TRUE(kg_->add_entity(entity));

    // 获取实体
    auto result = kg_->get_entity("e1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "测试实体");
    EXPECT_EQ(result->type, EntityType::TECHNOLOGY);
}

TEST_F(KnowledgeGraphTest, GetEntityByName) {
    KGEntity entity;
    entity.id = "e2";
    entity.name = "知识图谱";
    entity.type = EntityType::CONCEPT;

    kg_->add_entity(entity);

    auto result = kg_->get_entity_by_name("知识图谱");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id, "e2");
}

TEST_F(KnowledgeGraphTest, UpdateEntity) {
    KGEntity entity;
    entity.id = "e3";
    entity.name = "原始名称";
    entity.type = EntityType::PERSON;

    kg_->add_entity(entity);

    // 更新实体
    entity.description = "更新后的描述";
    entity.aliases.push_back("别名");
    EXPECT_TRUE(kg_->update_entity(entity));

    auto result = kg_->get_entity("e3");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->description, "更新后的描述");
    EXPECT_EQ(result->aliases.size(), 1);
}

TEST_F(KnowledgeGraphTest, DeleteEntity) {
    KGEntity entity;
    entity.id = "e4";
    entity.name = "待删除实体";

    kg_->add_entity(entity);
    EXPECT_TRUE(kg_->has_entity("e4"));

    kg_->delete_entity("e4");
    EXPECT_FALSE(kg_->has_entity("e4"));
}

// ========== 关系测试 ==========

TEST_F(KnowledgeGraphTest, AddAndGetRelation) {
    // 先添加两个实体
    KGEntity e1{"e1", "实体1", EntityType::PERSON};
    KGEntity e2{"e2", "实体2", EntityType::ORGANIZATION};
    kg_->add_entity(e1);
    kg_->add_entity(e2);

    // 添加关系
    KGRelation rel;
    rel.id = "r1";
    rel.source_id = "e1";
    rel.target_id = "e2";
    rel.type = RelationTypes::WORKS_AT;
    rel.weight = 1.0f;

    EXPECT_TRUE(kg_->add_relation(rel));

    // 验证关系
    auto result = kg_->get_relation("r1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->source_id, "e1");
    EXPECT_EQ(result->target_id, "e2");
}

TEST_F(KnowledgeGraphTest, GetNeighbors) {
    // 创建星形结构
    KGEntity center{"center", "中心实体", EntityType::CONCEPT};
    KGEntity n1{"n1", "邻居1", EntityType::PERSON};
    KGEntity n2{"n2", "邻居2", EntityType::PERSON};

    kg_->add_entity(center);
    kg_->add_entity(n1);
    kg_->add_entity(n2);

    kg_->add_relation({"r1", "center", "n1", RelationTypes::RELATED_TO});
    kg_->add_relation({"r2", "center", "n2", RelationTypes::RELATED_TO});

    auto neighbors = kg_->get_neighbors("center");
    EXPECT_EQ(neighbors.size(), 2);
}

TEST_F(KnowledgeGraphTest, GetReachableEntities) {
    KGEntity a{"a", "A", EntityType::PERSON};
    KGEntity b{"b", "B", EntityType::PERSON};
    KGEntity c{"c", "C", EntityType::PERSON};
    KGEntity d{"d", "D", EntityType::PERSON};

    kg_->add_entity(a);
    kg_->add_entity(b);
    kg_->add_entity(c);
    kg_->add_entity(d);

    // a -> b -> c -> d
    kg_->add_relation({"r1", "a", "b", RelationTypes::RELATED_TO});
    kg_->add_relation({"r2", "b", "c", RelationTypes::RELATED_TO});
    kg_->add_relation({"r3", "c", "d", RelationTypes::RELATED_TO});

    // 1 跳可达
    auto r1 = kg_->get_reachable_entities("a", 1);
    EXPECT_EQ(r1.size(), 1);
    EXPECT_TRUE(r1.find("b") != r1.end());

    // 2 跳可达
    auto r2 = kg_->get_reachable_entities("a", 2);
    EXPECT_TRUE(r2.find("b") != r2.end());
    EXPECT_TRUE(r2.find("c") != r2.end());
}

// ========== 最短路径测试 ==========

TEST_F(KnowledgeGraphTest, FindShortestPath) {
    KGEntity a{"a", "A", EntityType::PERSON};
    KGEntity b{"b", "B", EntityType::PERSON};
    KGEntity c{"c", "C", EntityType::PERSON};
    KGEntity d{"d", "D", EntityType::PERSON};

    kg_->add_entity(a);
    kg_->add_entity(b);
    kg_->add_entity(c);
    kg_->add_entity(d);

    // a -> b -> c -> d
    kg_->add_relation({"r1", "a", "b", RelationTypes::RELATED_TO});
    kg_->add_relation({"r2", "b", "c", RelationTypes::RELATED_TO});
    kg_->add_relation({"r3", "c", "d", RelationTypes::RELATED_TO});

    auto path = kg_->find_shortest_path("a", "d");
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->entities.size(), 4);
    EXPECT_EQ(path->relations.size(), 3);
}

TEST_F(KnowledgeGraphTest, NoPathExists) {
    KGEntity a{"a", "A", EntityType::PERSON};
    KGEntity b{"b", "B", EntityType::PERSON};

    kg_->add_entity(a);
    kg_->add_entity(b);
    // 不添加关系

    auto path = kg_->find_shortest_path("a", "b");
    EXPECT_FALSE(path.has_value());
}

// ========== 子图测试 ==========

TEST_F(KnowledgeGraphTest, GetSubgraph) {
    // 创建复杂图结构
    KGEntity root{"root", "根实体", EntityType::CONCEPT};
    KGEntity l1{"l1", "层级1实体", EntityType::PERSON};
    KGEntity l2a{"l2a", "层级2A", EntityType::ORGANIZATION};
    KGEntity l2b{"l2b", "层级2B", EntityType::LOCATION};
    KGEntity l3{"l3", "层级3实体", EntityType::TECHNOLOGY};

    kg_->add_entity(root);
    kg_->add_entity(l1);
    kg_->add_entity(l2a);
    kg_->add_entity(l2b);
    kg_->add_entity(l3);

    // root -> l1 -> l2a, l2b
    // l2a -> l3
    kg_->add_relation({"r1", "root", "l1", RelationTypes::RELATED_TO});
    kg_->add_relation({"r2", "l1", "l2a", RelationTypes::RELATED_TO});
    kg_->add_relation({"r3", "l1", "l2b", RelationTypes::RELATED_TO});
    kg_->add_relation({"r4", "l2a", "l3", RelationTypes::RELATED_TO});

    // 2 跳子图
    auto subgraph = kg_->get_subgraph("root", 2);
    EXPECT_EQ(subgraph.entities.size(), 4);  // root, l1, l2a, l2b
    EXPECT_EQ(subgraph.relations.size(), 3);
}

// ========== 统计测试 ==========

TEST_F(KnowledgeGraphTest, Statistics) {
    KGEntity e1{"e1", "实体1", EntityType::PERSON};
    KGEntity e2{"e2", "实体2", EntityType::ORGANIZATION};
    KGEntity e3{"e3", "实体3", EntityType::LOCATION};

    kg_->add_entity(e1);
    kg_->add_entity(e2);
    kg_->add_entity(e3);

    kg_->add_relation({"r1", "e1", "e2", RelationTypes::WORKS_AT});
    kg_->add_relation({"r2", "e2", "e3", RelationTypes::LOCATED_IN});

    EXPECT_EQ(kg_->entity_count(), 3);
    EXPECT_EQ(kg_->relation_count(), 2);
    EXPECT_GT(kg_->density(), 0.0f);
}

// ========== 实体提取器测试 ==========

class EntityExtractorTest : public ::testing::Test {
protected:
    void SetUp() override {
        extractor_ = create_entity_extractor("rule");
    }

    std::unique_ptr<EntityExtractor> extractor_;
};

TEST_F(EntityExtractorTest, BasicExtraction) {
    std::string text = "张三在北京工作，他在北京大学任教。";
    auto result = extractor_->extract(text, "chunk1");

    // 应该提取到实体
    EXPECT_GT(result.entities.size(), 0);

    // 验证处理时间已设置
    EXPECT_GE(result.processing_time_ms, 0);
}

TEST_F(EntityExtractorTest, BatchExtraction) {
    std::vector<std::pair<std::string, std::string>> texts = {
        {"张三在北京工作", "chunk1"},
        {"李四是一名教授", "chunk2"}
    };

    auto results = extractor_->extract_batch(texts);
    EXPECT_EQ(results.size(), 2);
}

TEST_F(EntityExtractorTest, RuleBasedExtractorName) {
    EXPECT_EQ(extractor_->name(), "rule-based");
}
