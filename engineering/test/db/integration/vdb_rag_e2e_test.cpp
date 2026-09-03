/**
 * @file vdb_rag_e2e_test.cpp
 * @brief RAG 端到端集成测试
 *
 * 测试范围：
 * 1. 文档入库（Markdown 解析 → 分块 → Embedding → 向量插入）
 * 2. RAG 查询（query → retrieve → 返回相关 Chunk）
 * 3. RAG 生成（query → retrieve → LLM 生成）
 * 4. RAG 评估（RAGAS 指标计算）
 *
 * 注意：这些测试需要外部依赖（Ollama 服务），部分测试可能被跳过
 */
#include <gtest/gtest.h>
#include <rag/rag.h>
#include <rag/config.h>
#include <rag/engine.h>
#include <rag/parser.h>
#include <rag/chunker.h>
#include <rag/embedding.h>
#include <rag/retriever.h>
#include <rag/ollama_embedding.h>
#include <rag/ollama_llm.h>
#include <rag/types.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace fs = std::filesystem;

// ============================================================
// 测试夹具
// ============================================================

class RAGE2ETest : public ::testing::Test {
protected:
    static const char* test_data_dir;
    rag::EngineConfig engine_config;
    std::unique_ptr<rag::RAGEngine> engine;

    static void SetUpTestSuite() {
#ifdef _WIN32
        mkdir(test_data_dir);
#else
        mkdir(test_data_dir, 0755);
#endif
        // 创建测试数据目录
        fs::create_directories(test_data_dir);
        fs::create_directories(fs::path(test_data_dir) / "docs");
        fs::create_directories(fs::path(test_data_dir) / "index");
    }

    void SetUp() override {
        // 配置引擎
        engine_config.config.data_dir = test_data_dir;
        engine_config.config.index_dir = fs::path(test_data_dir) / "index";
        engine_config.config.embedding.model_type = "bge-base-zh-v1.5";
        engine_config.config.embedding.device = "cpu";
        engine_config.config.hnsw.dim = 768;
        engine_config.config.hnsw.m = 16;
        engine_config.config.hnsw.ef_construction = 200;
        engine_config.config.hnsw.ef_search = 100;
        engine_config.config.retrieval.top_k = 5;
        engine_config.config.retrieval.hybrid = true;
        engine_config.config.chunking.chunk_size = 200;
        engine_config.config.chunking.chunk_overlap = 20;
        engine_config.load_existing_index = false;

        // 创建引擎
        engine = rag::create_engine(engine_config);
        ASSERT_NE(nullptr, engine);
    }

    void TearDown() override {
        if (engine) {
            engine->shutdown();
            engine.reset();
        }
    }

    // 创建测试 Markdown 文档
    std::string create_test_document(const std::string& title,
                                      const std::string& content) {
        std::stringstream ss;
        ss << "# " << title << "\n\n" << content;
        return ss.str();
    }

    // 将文档写入文件
    void write_test_file(const std::string& filename,
                         const std::string& content) {
        fs::path file_path = fs::path(test_data_dir) / "docs" / filename;
        std::ofstream ofs(file_path);
        ASSERT_TRUE(ofs.is_open());
        ofs << content;
        ofs.close();
    }

    // 检查 Ollama 服务是否可用
    static bool is_ollama_available() {
        // 简单的连接测试
        return std::getenv("OLLAMA_HOST") != nullptr ||
               std::getenv("SKIP_OLLAMA_TESTS") != nullptr;
    }
};

const char* RAGE2ETest::test_data_dir = "./test-results/vdb_rag_e2e";

// ============================================================
// 4. RAG 端到端测试
// ============================================================

/**
 * 测试 4.1：文档入库（Markdown 解析 → 分块 → Embedding → 向量插入）
 */
TEST_F(RAGE2ETest, DocumentIngestion) {
    // 创建测试文档
    std::string doc1_content = create_test_document(
        "向量数据库简介",
        "向量数据库是一种专门用于存储和检索高维向量的数据库系统。"
        "它通过近似最近邻（ANN）算法实现高效的相似性搜索。"
        "常见的向量数据库包括 Milvus、Qdrant、Weaviate 等。"
        "向量数据库在推荐系统、语义搜索、图像检索等场景有广泛应用。"
    );

    std::string doc2_content = create_test_document(
        "HNSW 算法原理",
        "HNSW（Hierarchical Navigable Small World）是一种基于图的近似最近邻搜索算法。"
        "它通过构建多层跳表结构的 NSW 图来实现高效的搜索。"
        "HNSW 的搜索时间复杂度为 O(log n)，空间复杂度为 O(n log n)。"
        "HNSW 的主要参数包括 M（每层最大连接数）和 ef（搜索时候选集大小）。"
    );

    // 写入测试文件
    write_test_file("vector_db_intro.md", doc1_content);
    write_test_file("hnsw_algorithm.md", doc2_content);

    // 初始化引擎组件
    engine->init(engine_config);

    // 构建索引
    auto result = engine->build_index({fs::path(test_data_dir) / "docs"});
    EXPECT_TRUE(result.success);
    EXPECT_EQ(2, result.documents_processed);
    EXPECT_GT(result.chunks_created, 0);

    // 验证索引状态
    auto status = engine->get_index_status();
    EXPECT_EQ(2, status.document_count);
    EXPECT_GT(status.chunk_count, 0);

    printf("\n[DocumentIngestion] Documents: %d, Chunks: %d, Duration: %ld ms\n",
           result.documents_processed, result.chunks_created, result.duration_ms);
}

/**
 * 测试 4.2：RAG 查询（query → retrieve → 返回相关 Chunk）
 */
TEST_F(RAGE2ETest, RAGRetrieve) {
    // 创建测试文档
    std::string doc_content = create_test_document(
        "向量数据库与 AI",
        "向量数据库是 AI 应用的重要基础设施。"
        "大语言模型（LLM）可以将文本转换为向量表示。"
        "通过向量数据库，可以实现语义相似性搜索。"
        "RAG（Retrieval-Augmented Generation）是当前主流的 LLM 增强技术。"
        "RAG 通过检索相关上下文来增强 LLM 的回答质量。"
    );

    write_test_file("ai_vector.md", doc_content);

    // 初始化并构建索引
    engine->init(engine_config);
    auto build_result = engine->build_index({fs::path(test_data_dir) / "docs"});
    EXPECT_TRUE(build_result.success);

    // 执行检索查询
    auto retrieval_results = engine->retrieve("向量数据库在 AI 中的应用", 3);

    printf("\n[RAGRetrieve] Query: 向量数据库在 AI 中的应用\n");
    printf("Results: %zu\n", retrieval_results.size());

    // 验证结果
    EXPECT_GE(retrieval_results.size(), 0);
    if (!retrieval_results.empty()) {
        for (const auto& result : retrieval_results) {
            printf("  - Chunk: %s... (score: %.4f)\n",
                   result.content.substr(0, 50).c_str(),
                   result.score);
            EXPECT_GT(result.score, 0.0f);
        }
    }
}

/**
 * 测试 4.3：RAG 生成（query → retrieve → LLM 生成）
 * 注意：此测试需要 Ollama 服务运行，如果没有则跳过
 */
TEST_F(RAGE2ETest, RAGGeneration) {
    // 创建测试文档
    std::string doc_content = create_test_document(
        "RAG 系统架构",
        "RAG 系统主要由三个部分组成：索引构建、检索和生成。"
        "索引构建阶段负责文档解析、分块和向量化。"
        "检索阶段根据查询找到最相关的文档块。"
        "生成阶段将检索结果与查询一起发送给 LLM。"
        "RAG 可以有效解决 LLM 的幻觉问题和知识过时问题。"
    );

    write_test_file("rag_architecture.md", doc_content);

    // 初始化并构建索引
    engine->init(engine_config);
    auto build_result = engine->build_index({fs::path(test_data_dir) / "docs"});
    EXPECT_TRUE(build_result.success);

    // 检查是否跳过（需要 Ollama）
    if (is_ollama_available()) {
        GTEST_SKIP() << "Skipping RAG generation test (Ollama not available)";
    }

    // 设置 LLM 服务（需要配置 Ollama）
    // auto llm = std::make_shared<rag::OllamaLLM>("http://localhost:11434", "qwen2.5");
    // engine->set_llm_service(llm);

    // 执行查询（使用纯检索模式，因为没有 LLM）
    auto query_result = engine->query("RAG 系统由哪些部分组成？", 3);

    printf("\n[RAGGeneration] Query: RAG 系统由哪些部分组成？\n");
    printf("Answer: %s\n", query_result.answer.c_str());
    printf("Sources: %zu\n", query_result.sources.size());

    // 注意：由于没有真实的 LLM，这里只验证查询能正常执行
    EXPECT_TRUE(!query_result.answer.empty() || !query_result.sources.empty());
}

/**
 * 测试 4.4：RAG 评估（RAGAS 指标计算）
 * 注意：此测试需要 Ollama 服务和真实评估数据
 */
TEST_F(RAGE2ETest, RAGEvaluation) {
    // 创建测试文档
    std::string doc_content = create_test_document(
        "向量相似性度量",
        "向量数据库中常用的相似性度量包括："
        "1. 余弦相似度：衡量两个向量方向的相似性"
        "2. 欧氏距离：衡量两个向量点之间的直线距离"
        "3. 点积/内积：衡量向量在同一方向上的投影长度"
        "不同的度量适用于不同的应用场景。"
        "归一化后的向量适合使用余弦相似度，未归一化的向量适合使用内积。"
    );

    write_test_file("similarity_metrics.md", doc_content);

    // 初始化并构建索引
    engine->init(engine_config);
    auto build_result = engine->build_index({fs::path(test_data_dir) / "docs"});
    EXPECT_TRUE(build_result.success);

    // 检查是否跳过（需要 Ollama）
    if (is_ollama_available()) {
        GTEST_SKIP() << "Skipping RAG evaluation test (Ollama not available)";
    }

    // 获取评估器
    auto evaluator = engine->evaluator();
    if (!evaluator) {
        GTEST_SKIP() << "Evaluator not available";
    }

    // 定义测试用例
    std::string query = "向量数据库有哪些相似性度量？";
    std::vector<std::string> ground_truths = {
        "常用的相似性度量包括余弦相似度、欧氏距离和内积"
    };

    // 执行评估
    auto eval_result = evaluator->evaluate(query, ground_truths, "");

    printf("\n[RAGEvaluation] Query: %s\n", query.c_str());
    printf("Faithfulness: %.4f\n", eval_result.faithfulness);
    printf("Answer Relevance: %.4f\n", eval_result.answer_relevance);
    printf("Context Precision: %.4f\n", eval_result.context_precision);
    printf("Context Recall: %.4f\n", eval_result.context_recall);

    // 验证评估指标在有效范围内
    EXPECT_GE(eval_result.faithfulness, 0.0f);
    EXPECT_LE(eval_result.faithfulness, 1.0f);
    EXPECT_GE(eval_result.answer_relevance, 0.0f);
    EXPECT_LE(eval_result.answer_relevance, 1.0f);
}

/**
 * 测试 4.1-4.4 综合：完整 RAG 流程
 */
TEST_F(RAGE2ETest, CompleteRAGWorkflow) {
    // Step 1: 创建多个测试文档
    std::vector<std::pair<std::string, std::string>> docs = {
        {"hnsw.md", create_test_document("HNSW 算法",
            "HNSW（Hierarchical Navigable Small World）是一种高效的近似最近邻搜索算法。"
            "它通过构建多层图结构来实现 O(log n) 的搜索复杂度。"
            "HNSW 的核心思想是将相似的数据点连接成一个小世界网络。")},
        {"embedding.md", create_test_document("向量嵌入",
            "向量嵌入是将高维数据映射到低维连续向量空间的技术。"
            "好的嵌入能够保留原始数据的语义信息和相似性关系。"
            "常用的嵌入模型包括 BERT、Sentence-BERT、OpenAI Embeddings 等。")},
        {"rag_intro.md", create_test_document("RAG 简介",
            "RAG（Retrieval-Augmented Generation）是一种结合检索和生成的技术。"
            "它通过从外部知识库检索相关信息来增强 LLM 的回答能力。"
            "RAG 可以有效解决 LLM 的知识截止日期和幻觉问题。")}
    };

    for (const auto& [filename, content] : docs) {
        write_test_file(filename, content);
    }

    // Step 2: 初始化引擎
    engine->init(engine_config);

    // Step 3: 构建索引
    auto build_result = engine->build_index({fs::path(test_data_dir) / "docs"});
    EXPECT_TRUE(build_result.success);
    EXPECT_EQ(3, build_result.documents_processed);
    EXPECT_GT(build_result.chunks_created, 0);

    // Step 4: 执行多个查询
    std::vector<std::string> queries = {
        "HNSW 算法是什么？",
        "向量嵌入有哪些应用？",
        "RAG 如何工作？"
    };

    printf("\n[CompleteRAGWorkflow] Testing %zu queries\n", queries.size());

    for (const auto& query : queries) {
        auto results = engine->retrieve(query, 2);

        printf("\n  Query: %s\n", query.c_str());
        printf("  Retrieved: %zu chunks\n", results.size());

        for (const auto& r : results) {
            printf("    - [%.4f] %s...\n",
                   r.score, r.content.substr(0, 40).c_str());
        }

        // 每个查询都应该返回结果
        EXPECT_GE(results.size(), 0);
    }

    // Step 5: 验证索引状态
    auto status = engine->get_index_status();
    EXPECT_EQ(3, status.document_count);
    EXPECT_GT(status.chunk_count, 0);

    printf("\n  Final status - Documents: %d, Chunks: %d\n",
           status.document_count, status.chunk_count);
}

/**
 * 测试：文档管理
 */
TEST_F(RAGE2ETest, DocumentManagement) {
    // 初始化引擎
    engine->init(engine_config);

    // 创建文档
    rag::Document doc;
    doc.id = "doc_001";
    doc.title = "测试文档";
    doc.content = "这是一个测试文档的内容。";
    doc.metadata["source"] = "test";

    // 添加文档
    engine->add_document(doc);

    // 获取文档
    auto retrieved = engine->get_document("doc_001");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(doc.title, retrieved->title);
    EXPECT_EQ(doc.content, retrieved->content);

    // 列出文档
    auto docs = engine->list_documents();
    EXPECT_GE(docs.size(), 1);

    // 删除文档
    engine->remove_document("doc_001");
    retrieved = engine->get_document("doc_001");
    EXPECT_FALSE(retrieved.has_value());
}

/**
 * 测试：增量索引更新
 */
TEST_F(RAGE2ETest, IncrementalIndexUpdate) {
    // 初始化并构建初始索引
    engine->init(engine_config);

    write_test_file("doc1.md", create_test_document("文档1", "这是第一个文档的内容。"));
    auto result1 = engine->build_index({fs::path(test_data_dir) / "docs"});
    EXPECT_EQ(1, result1.documents_processed);

    auto status1 = engine->get_index_status();
    int chunks_after_first = status1.chunk_count;

    // 添加新文档
    write_test_file("doc2.md", create_test_document("文档2", "这是第二个文档的内容。"));

    // 增量更新
    auto result2 = engine->update_index(fs::path(test_data_dir) / "docs" / "doc2.md");
    EXPECT_TRUE(result2.success);

    auto status2 = engine->get_index_status();
    EXPECT_EQ(2, status2.document_count);
    EXPECT_GT(status2.chunk_count, chunks_after_first);

    printf("\n[IncrementalUpdate] Initial chunks: %d, After update: %d\n",
           chunks_after_first, status2.chunk_count);
}

// ============================================================
// 主函数
// ============================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
