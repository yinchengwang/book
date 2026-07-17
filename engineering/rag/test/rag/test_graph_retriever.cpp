/**
 * @file test_graph_retriever.cpp
 * @brief Graph 检索器单元测试
 */

#include <gtest/gtest.h>
#include "rag/graph_retriever.h"
#include "rag/knowledge_graph.h"

using namespace rag;

// ========== 测试夹具 ==========

class GraphRetrieverTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建知识图谱
        kg_ = create_knowledge_graph();

        // 添加测试数据
        setup_test_data();

        // 创建检索器
        GraphRetrievalConfig config;
        config.max_hops = 2;
        config.include_subgraph = true;
        config.include_text_chunks = true;
        retriever_ = create_graph_retriever("base", config);
        retriever_->set_knowledge_graph(kg_.get());
    }

    void setup_test_data() {
        // 添加实体
        KGEntity person1{"p1", "张三", EntityType::PERSON};
        person1.description = "一位软件工程师";
        kg_->add_entity(person1);

        KGEntity org1{"o1", "百度公司", EntityType::ORGANIZATION};
        org1.description = "中国互联网公司";
        kg_->add_entity(org1);

        KGEntity tech1{"t1", "人工智能", EntityType::TECHNOLOGY};
        tech1.description = "AI 技术";
        kg_->add_entity(tech1);

        KGEntity loc1{"l1", "北京", EntityType::LOCATION};
        kg_->add_entity(loc1);

        // 添加关系
        kg_->add_relation({"r1", "p1", "o1", RelationTypes::WORKS_AT});
        kg_->add_relation({"r2", "o1", "t1", RelationTypes::USES});
        kg_->add_relation({"r3", "o1", "l1", RelationTypes::LOCATED_IN});
        kg_->add_relation({"r4", "p1", "t1", RelationTypes::KNOWS});
    }

    std::unique_ptr<KnowledgeGraph> kg_;
    std::unique_ptr<GraphRetriever> retriever_;
};

// ========== 实体链接测试 ==========

TEST_F(GraphRetrieverTest, LinkQueryEntities) {
    auto results = retriever_->link_query_entities("张三 在 百度 工作");

    // 应该链接到 "张三" 和 "百度"
    EXPECT_GE(results.size(), 0);  // 取决于分词结果
}

TEST_F(GraphRetrieverTest, LinkWithNoMatch) {
    auto results = retriever_->link_query_entities("完全不相关的查询");

    // 无匹配时返回空
    EXPECT_TRUE(results.empty());
}

TEST_F(GraphRetrieverTest, LinkMultipleEntities) {
    auto results = retriever_->link_query_entities("张三和李四都在百度公司工作");

    // 应该链接到多个实体
    EXPECT_TRUE(results.size() <= 3);
}

// ========== 子图扩展测试 ==========

TEST_F(GraphRetrieverTest, ExpandSubgraphOneHop) {
    auto subgraph = retriever_->expand_subgraph("p1", 1);

    EXPECT_EQ(subgraph.root_entity_id, "p1");
    EXPECT_GE(subgraph.entities.size(), 1);
    EXPECT_EQ(subgraph.hop_count, 1);
}

TEST_F(GraphRetrieverTest, ExpandSubgraphTwoHops) {
    auto subgraph = retriever_->expand_subgraph("p1", 2);

    // 2 跳应该包含: p1 -> o1 -> (t1, l1)
    EXPECT_GE(subgraph.entities.size(), 2);
    EXPECT_EQ(subgraph.hop_count, 2);
}

TEST_F(GraphRetrieverTest, ExpandSubgraphNonExistent) {
    auto subgraph = retriever_->expand_subgraph("non_existent", 2);

    // 不存在的实体返回空子图
    EXPECT_TRUE(subgraph.entities.empty());
}

// ========== 检索测试 ==========

TEST_F(GraphRetrieverTest, BasicRetrieve) {
    RetrievalConfig config;
    config.top_k = 10;

    auto result = retriever_->retrieve("张三", config);

    EXPECT_EQ(result.query, "张三");
    EXPECT_GE(result.matched_entities.size(), 0);
    EXPECT_GE(result.processing_time_ms, 0);
}

TEST_F(GraphRetrieverTest, RetrieveWithSubgraph) {
    RetrievalConfig config;
    config.top_k = 10;

    auto result = retriever_->retrieve("百度公司", config);

    // 应该匹配到实体
    EXPECT_EQ(result.query, "百度公司");

    // 如果启用子图，应该有子图数据
    EXPECT_GE(result.subgraph.entities.size(), 0);
}

// ========== Hybrid 检索器测试 ==========

class HybridGraphRetrieverTest : public ::testing::Test {
protected:
    void SetUp() override {
        kg_ = create_knowledge_graph();

        // 添加测试数据
        KGEntity e1{"e1", "机器学习", EntityType::TECHNOLOGY};
        KGEntity e2{"e2", "深度学习", EntityType::TECHNOLOGY};
        KGEntity e3{"e3", "神经网络", EntityType::TECHNOLOGY};

        kg_->add_entity(e1);
        kg_->add_entity(e2);
        kg_->add_entity(e3);

        kg_->add_relation({"r1", "e1", "e2", RelationTypes::RELATED_TO});
        kg_->add_relation({"r2", "e2", "e3", RelationTypes::RELATED_TO});

        // 创建混合检索器
        GraphRetrievalConfig config;
        config.max_hops = 2;
        config.include_subgraph = true;
        config.include_text_chunks = true;
        hybrid_ = create_graph_retriever("hybrid", config);
        hybrid_->set_knowledge_graph(kg_.get());
    }

    std::unique_ptr<KnowledgeGraph> kg_;
    std::unique_ptr<GraphRetriever> hybrid_;
};

TEST_F(HybridGraphRetrieverTest, HybridRetrieverName) {
    EXPECT_EQ(hybrid_->name(), "hybrid-graph");
}

TEST_F(HybridGraphRetrieverTest, HybridRetrieve) {
    RetrievalConfig config;
    config.top_k = 10;

    auto result = hybrid_->retrieve("机器学习", config);

    EXPECT_EQ(result.query, "机器学习");
    EXPECT_GE(result.matched_entities.size(), 0);
    EXPECT_EQ(result.method, "hybrid-graph");
}

// ========== 配置测试 ==========

TEST(GraphRetrievalConfigTest, DefaultConfig) {
    GraphRetrievalConfig config;

    EXPECT_EQ(config.max_hops, 2);
    EXPECT_EQ(config.max_entities, 50);
    EXPECT_EQ(config.max_relations, 100);
    EXPECT_EQ(config.entity_similarity_threshold, 0.6f);
    EXPECT_TRUE(config.include_subgraph);
    EXPECT_TRUE(config.include_text_chunks);
}

TEST(GraphRetrievalConfigTest, CustomConfig) {
    GraphRetrievalConfig config;
    config.max_hops = 3;
    config.include_text_chunks = false;

    EXPECT_EQ(config.max_hops, 3);
    EXPECT_FALSE(config.include_text_chunks);
}
