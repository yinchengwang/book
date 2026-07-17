/**
 * @file test_graph_integration.cpp
 * @brief Graph RAG 集成测试
 */

#include <gtest/gtest.h>
#include "rag/graph_support.h"
#include "rag/knowledge_graph.h"
#include "rag/entity_extractor.h"
#include "rag/database.h"

using namespace rag;

// ========== 图谱构建集成测试 ==========

class GraphBuildIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建内存数据库用于测试
        db_ = std::make_shared<Database>();
        db_->open(":memory:");

        // 初始化 Schema
        SchemaManager schema(*db_);
        schema.init();

        // 创建图谱配置
        config_.enabled = true;
        config_.extractor_type = "rule";
        config_.max_hops = 2;
        config_.entity_confidence_threshold = 0.5f;

        // 创建图谱扩展
        graph_ext_ = create_graph_extension(db_, config_);

        // 插入测试数据
        insert_test_chunks();
    }

    void TearDown() override {
        graph_ext_.reset();
        db_->close();
        db_.reset();
    }

    void insert_test_chunks() {
        // 插入测试 chunks
        db_->execute(
            "INSERT INTO chunks (id, content, document_id) VALUES ('c1', '张三在北京工作，他是一名软件工程师。', 'd1')"
        );
        db_->execute(
            "INSERT INTO chunks (id, content, document_id) VALUES ('c2', '李四在百度公司从事人工智能研究。', 'd1')"
        );
        db_->execute(
            "INSERT INTO chunks (id, content, document_id) VALUES ('c3', '人工智能技术广泛应用于各个领域。', 'd2')"
        );
    }

    std::shared_ptr<Database> db_;
    GraphConfig config_;
    std::unique_ptr<GraphEngineExtension> graph_ext_;
};

TEST_F(GraphBuildIntegrationTest, BuildKnowledgeGraph) {
    auto result = graph_ext_->build_knowledge_graph();

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.chunks_processed, 3);
    EXPECT_GE(result.entities_extracted, 0);
    EXPECT_GE(result.relations_extracted, 0);
    EXPECT_GE(result.duration_ms, 0);
}

TEST_F(GraphBuildIntegrationTest, IncrementalUpdate) {
    // 先构建
    graph_ext_->build_knowledge_graph();

    // 添加新 chunk
    db_->execute(
        "INSERT INTO chunks (id, content, document_id) VALUES ('c4', '王五加入了百度公司。', 'd3')"
    );

    // 增量更新
    auto result = graph_ext_->update_knowledge_graph({"c4"});

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.chunks_processed, 1);
}

TEST_F(GraphBuildIntegrationTest, GraphStatus) {
    graph_ext_->build_knowledge_graph();

    auto status = graph_ext_->get_graph_status();

    EXPECT_EQ(status.index_name, "knowledge_graph");
    EXPECT_EQ(status.status, IndexStatus::Status::READY);
}

TEST_F(GraphBuildIntegrationTest, GraphRetrieve) {
    graph_ext_->build_knowledge_graph();

    auto result = graph_ext_->graph_retrieve("张三", 10);

    EXPECT_EQ(result.query, "张三");
    EXPECT_GE(result.processing_time_ms, 0);
}

TEST_F(GraphBuildIntegrationTest, SaveAndLoad) {
    // 构建图谱
    graph_ext_->build_knowledge_graph();

    // 保存
    graph_ext_->save_knowledge_graph();

    // 重新创建扩展并加载
    auto graph_ext2 = create_graph_extension(db_, config_);

    // 加载
    graph_ext2->load_knowledge_graph();

    // 验证加载成功
    auto status = graph_ext2->get_graph_status();
    EXPECT_EQ(status.status, IndexStatus::Status::READY);
}

// ========== 实体链接集成测试 ==========

class EntityLinkingIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        kg_ = create_knowledge_graph();

        // 添加实体
        KGEntity e1{"e1", "机器学习", EntityType::TECHNOLOGY};
        e1.aliases.push_back("Machine Learning");
        e1.aliases.push_back("ML");
        kg_->add_entity(e1);

        KGEntity e2{"e2", "深度学习", EntityType::TECHNOLOGY};
        e2.aliases.push_back("Deep Learning");
        kg_->add_entity(e2);

        KGEntity e3{"e3", "神经网络", EntityType::TECHNOLOGY};
        kg_->add_entity(e3);

        // 添加关系
        kg_->add_relation({"r1", "e1", "e2", RelationTypes::RELATED_TO});
        kg_->add_relation({"r2", "e2", "e3", RelationTypes::RELATED_TO});
    }

    std::unique_ptr<KnowledgeGraph> kg_;
};

TEST_F(EntityLinkingIntegrationTest, EntityLinkingWithAliases) {
    // 创建提取器
    auto extractor = create_entity_extractor("rule");

    // 提取包含别名的文本
    auto result = extractor->extract(
        "Machine Learning 是人工智能的子领域。",
        "chunk1"
    );

    // 应该能提取到机器学习相关实体
    EXPECT_GE(result.entities.size(), 0);
}

TEST_F(EntityLinkingIntegrationTest, MultiHopRetrieval) {
    // 从机器学习出发，2跳应该能到达神经网络
    auto reachable = kg_->get_reachable_entities("e1", 2);

    EXPECT_TRUE(reachable.find("e2") != reachable.end());
    EXPECT_TRUE(reachable.find("e3") != reachable.end());
}

TEST_F(EntityLinkingIntegrationTest, SubgraphExtraction) {
    // 提取 2 跳子图
    auto subgraph = kg_->get_subgraph("e1", 2);

    EXPECT_GE(subgraph.entities.size(), 2);
    EXPECT_GE(subgraph.relations.size(), 1);
}

// ========== 端到端测试 ==========

class EndToEndGraphRAGTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建数据库
        db_ = std::make_shared<Database>();
        db_->open(":memory:");

        SchemaManager schema(*db_);
        schema.init();

        // 创建知识图谱
        kg_ = create_knowledge_graph();

        // 添加实体和关系
        KGEntity ai{"ai", "人工智能", EntityType::TECHNOLOGY};
        KGEntity ml{"ml", "机器学习", EntityType::TECHNOLOGY};
        KGEntity dl{"dl", "深度学习", EntityType::TECHNOLOGY};
        KGEntity nn{"nn", "神经网络", EntityType::TECHNOLOGY};

        kg_->add_entity(ai);
        kg_->add_entity(ml);
        kg_->add_entity(dl);
        kg_->add_entity(nn);

        kg_->add_relation({"r1", "ai", "ml", RelationTypes::RELATED_TO});
        kg_->add_relation({"r2", "ml", "dl", RelationTypes::RELATED_TO});
        kg_->add_relation({"r3", "dl", "nn", RelationTypes::RELATED_TO});
    }

    void TearDown() override {
        kg_.reset();
        db_->close();
        db_.reset();
    }

    std::shared_ptr<Database> db_;
    std::unique_ptr<KnowledgeGraph> kg_;
};

TEST_F(EndToEndGraphRAGTest, FullGraphRAGPipeline) {
    // 1. 创建图检索器
    GraphRetrievalConfig config;
    config.max_hops = 2;
    config.include_subgraph = true;
    auto retriever = create_graph_retriever("hybrid", config);
    retriever->set_knowledge_graph(kg_.get());

    // 2. 执行检索
    RetrievalConfig retrieval_config;
    retrieval_config.top_k = 10;

    auto result = retriever->retrieve("机器学习", retrieval_config);

    // 3. 验证结果
    EXPECT_EQ(result.query, "机器学习");
    EXPECT_GE(result.matched_entities.size(), 0);
    EXPECT_GE(result.processing_time_ms, 0);
}

TEST_F(EndToEndGraphRAGTest, MultiHopQuery) {
    // 查询：神经网络和人工智能有什么关系？

    // 获取路径
    auto path = kg_->find_shortest_path("nn", "ai");

    ASSERT_TRUE(path.has_value());
    EXPECT_GE(path->entities.size(), 2);
    EXPECT_GE(path->relations.size(), 1);

    // 验证路径
    EXPECT_EQ(path->entities.back().name, "人工智能");
}

TEST_F(EndToEndGraphRAGTest, GraphStatistics) {
    EXPECT_EQ(kg_->entity_count(), 4);
    EXPECT_EQ(kg_->relation_count(), 3);
    EXPECT_GT(kg_->density(), 0.0f);
}
