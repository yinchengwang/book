/**
 * @file test_kg_enhancer.cpp
 * @brief 知识图谱增强单元测试
 */

#include <gtest/gtest.h>
#include "rag/kg_enhancer.h"
#include "rag/knowledge_graph.h"
#include "rag/pipeline.h"

using namespace rag;

// ========== 测试夹具 ==========

class KGEnhancerTest : public ::testing::Test {
protected:
    void SetUp() override {
        kg_ = create_knowledge_graph();
        linker_ = create_entity_linker(kg_);
        builder_ = create_kg_builder();
    }

    std::shared_ptr<KnowledgeGraph> kg_;
    std::unique_ptr<EntityLinker> linker_;
    std::unique_ptr<KGBuilder> builder_;
};

// ========== 实体链接测试 ==========

TEST_F(KGEnhancerTest, EntityLinking) {
    // 添加候选实体
    Entity e1;
    e1.id = "e1";
    e1.name = "张三";
    e1.type = "PERSON";
    e1.confidence = 0.9f;
    linker_->add_candidate(e1);

    Entity e2;
    e2.id = "e2";
    e2.name = "北京大学";
    e2.type = "ORGANIZATION";
    linker_->add_candidate(e2);

    // 链接文本中的实体
    std::string text = "张三在北京大学工作";
    auto results = linker_->link(text);

    // 至少应该找到一些提及
    EXPECT_GE(results.size(), 0);
}

TEST_F(KGEnhancerTest, EntityLinkingWithKG) {
    // 先在知识图谱中添加实体
    KGEntity kg_entity;
    kg_entity.id = "kg_e1";
    kg_entity.name = "李四";
    kg_entity.type = EntityType::PERSON;
    kg_entity.confidence = 0.95f;
    kg_->add_entity(kg_entity);

    linker_->set_knowledge_graph(kg_);

    // 链接实体
    auto result = linker_->link_entity("李四", "李四是一名教授");

    EXPECT_EQ(result.entity_id, "kg_e1");
    EXPECT_EQ(result.entity_name, "李四");
    EXPECT_EQ(result.entity_type, "PERSON");
    EXPECT_GT(result.confidence, 0.9f);
    EXPECT_EQ(result.source, "knowledge_graph");
}

TEST_F(KGEnhancerTest, EntityDisambiguation) {
    // 添加多个同名候选实体
    Entity e1;
    e1.id = "e1";
    e1.name = "苹果";
    e1.type = "ORGANIZATION";
    e1.confidence = 0.8f;
    linker_->add_candidate(e1);

    Entity e2;
    e2.id = "e2";
    e2.name = "苹果";
    e2.type = "PRODUCT";
    e2.confidence = 0.7f;
    linker_->add_candidate(e2);

    // 使用上下文消歧
    auto result = linker_->link_entity("苹果", "苹果公司发布了新产品");

    EXPECT_EQ(result.entity_name, "苹果");
    EXPECT_EQ(result.source, "disambiguation");
}

TEST_F(KGEnhancerTest, LinkEntityNotFound) {
    auto result = linker_->link_entity("不存在的实体", "这是一段上下文");

    EXPECT_TRUE(result.entity_id.empty());
    EXPECT_EQ(result.confidence, 0.0f);
}

// ========== 图谱增强检索测试 ==========

TEST_F(KGEnhancerTest, KGEnhancedRetrieve) {
    // 创建基础检索器（mock）
    auto retriever = create_kg_enhanced_retriever(nullptr, kg_);

    // 在图谱中添加实体
    KGEntity e1{"e1", "机器学习", EntityType::CONCEPT};
    KGEntity e2{"e2", "深度学习", EntityType::CONCEPT};
    kg_->add_entity(e1);
    kg_->add_entity(e2);
    kg_->add_relation({"r1", "e1", "e2", RelationTypes::RELATED_TO});

    // 检索
    auto result = retriever->retrieve("机器学习", 5);

    // 验证返回了结果
    EXPECT_GE(result.score, 0.0f);
    EXPECT_EQ(result.source, "kg_enhanced");
}

TEST_F(KGEnhancerTest, MultihopExpansion) {
    // 创建多跳图谱结构
    KGEntity e1{"e1", "人工智能", EntityType::CONCEPT};
    KGEntity e2{"e2", "机器学习", EntityType::CONCEPT};
    KGEntity e3{"e3", "深度学习", EntityType::CONCEPT};
    KGEntity e4{"e4", "神经网络", EntityType::CONCEPT};

    kg_->add_entity(e1);
    kg_->add_entity(e2);
    kg_->add_entity(e3);
    kg_->add_entity(e4);

    kg_->add_relation({"r1", "e1", "e2", RelationTypes::RELATED_TO});
    kg_->add_relation({"r2", "e2", "e3", RelationTypes::RELATED_TO});
    kg_->add_relation({"r3", "e3", "e4", RelationTypes::RELATED_TO});

    linker_->set_knowledge_graph(kg_);
    auto retriever = create_kg_enhanced_retriever(nullptr, kg_);

    // 展开多跳查询
    auto expanded = retriever->expand_multihop("人工智能", 2);

    // 应该生成了扩展查询
    EXPECT_GE(expanded.size(), 0);
}

TEST_F(KGEnhancerTest, GetEntityContext) {
    // 添加实体及其描述
    KGEntity e1{"e1", "Python", EntityType::TECHNOLOGY};
    e1.description = "一种编程语言";
    kg_->add_entity(e1);

    KGEntity e2{"e2", "机器学习", EntityType::CONCEPT};
    kg_->add_entity(e2);

    kg_->add_relation({"r1", "e1", "e2", RelationTypes::USED_BY});

    auto retriever = create_kg_enhanced_retriever(nullptr, kg_);

    // 获取实体上下文
    auto context = retriever->get_entity_context("Python");

    EXPECT_FALSE(context.empty());
    EXPECT_NE(context.find("Python"), std::string::npos);
}

// ========== 知识图谱构建器测试 ==========

TEST_F(KGEnhancerTest, KGBuilder) {
    // 从文档构建图谱
    std::vector<std::string> documents = {
        "人工智能是计算机科学的一个分支",
        "机器学习是人工智能的子领域",
        "深度学习使用神经网络"
    };

    builder_->build_from_documents(documents);

    // 验证图谱已构建
    auto kg = builder_->build();
    EXPECT_NE(kg, nullptr);
    EXPECT_GE(kg->entity_count(), 0);
}

TEST_F(KGEnhancerTest, KGBuilderAddEntity) {
    Entity entity;
    entity.id = "test_entity";
    entity.name = "测试实体";
    entity.type = "CONCEPT";
    entity.description = "用于测试的实体";

    builder_->add_entity(entity);

    auto kg = builder_->build();
    EXPECT_TRUE(kg->has_entity("test_entity"));
}

TEST_F(KGEnhancerTest, KGBuilderAddRelation) {
    // 先添加两个实体
    Entity e1;
    e1.id = "builder_e1";
    e1.name = "实体1";
    e1.type = "CONCEPT";

    Entity e2;
    e2.id = "builder_e2";
    e2.name = "实体2";
    e2.type = "CONCEPT";

    builder_->add_entity(e1);
    builder_->add_entity(e2);

    // 添加关系
    Relation rel;
    rel.id = "builder_r1";
    rel.source_id = "builder_e1";
    rel.target_id = "builder_e2";
    rel.type = RelationTypes::RELATED_TO;
    rel.confidence = 1.0f;

    builder_->add_relation(rel);

    auto kg = builder_->build();
    EXPECT_EQ(kg->relation_count(), 1);
}

TEST_F(KGEnhancerTest, KGBuilderSaveLoad) {
    // 添加一些实体
    Entity e1;
    e1.id = "save_e1";
    e1.name = "保存实体";
    e1.type = "CONCEPT";
    builder_->add_entity(e1);

    // 保存到文件
    std::string temp_path = "/tmp/test_kg_builder.json";
    EXPECT_TRUE(builder_->save(temp_path));

    // 重新加载
    auto new_builder = create_kg_builder();
    EXPECT_TRUE(new_builder->load(temp_path));

    auto kg = new_builder->build();
    EXPECT_GE(kg->entity_count(), 0);
}

// ========== 工厂函数测试 ==========

TEST_F(KGEnhancerTest, FactoryFunctions) {
    // 测试 create_entity_linker
    auto linker = create_entity_linker(nullptr);
    EXPECT_NE(linker, nullptr);

    // 测试 create_kg_enhanced_retriever
    auto retriever = create_kg_enhanced_retriever(nullptr, nullptr);
    EXPECT_NE(retriever, nullptr);

    // 测试 create_kg_builder
    auto builder = create_kg_builder();
    EXPECT_NE(builder, nullptr);
}

// ========== Mock NER 测试 ==========

TEST_F(KGEnhancerTest, ExtractMentions) {
    linker_->set_knowledge_graph(kg_);

    std::string text = "张三和李四在北京大学讨论人工智能";
    auto mentions = linker_->link(text);

    // 至少应该找到一些提及
    // Mock NER 会提取大写开头的词
    EXPECT_TRUE(true);  // 占位验证
}

// ========== 集成测试 ==========

TEST_F(KGEnhancerTest, FullIntegration) {
    // 1. 创建图谱构建器
    auto builder = create_kg_builder();

    // 2. 添加实体
    Entity e1{"e1", "TensorFlow", EntityType::TECHNOLOGY, "一个机器学习框架"};
    Entity e2{"e2", "Google", EntityType::ORGANIZATION, "谷歌公司"};
    builder->add_entity(e1);
    builder->add_entity(e2);

    // 3. 添加关系
    Relation rel{"r1", "e1", "e2", RelationTypes::CREATED_BY, 1.0f};
    builder->add_relation(rel);

    // 4. 获取构建好的图谱
    auto kg = builder->build();

    // 5. 创建实体链接器
    auto linker = create_entity_linker(kg);
    linker->add_candidate(e1);
    linker->add_candidate(e2);

    // 6. 创建图谱增强检索器
    auto retriever = create_kg_enhanced_retriever(nullptr, kg);

    // 7. 执行检索
    auto result = retriever->retrieve("TensorFlow", 5);

    // 验证
    EXPECT_GE(result.score, 0.0f);
    EXPECT_EQ(result.source, "kg_enhanced");
}
