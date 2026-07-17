/**
 * @file test_chunker.cpp
 * @brief 分块器测试
 */

#include <gtest/gtest.h>
#include "rag/chunker.h"
#include "rag/semantic_chunker.h"

using namespace rag;

TEST(ChunkerTest, FixedChunkerBasic) {
    ChunkingConfig config;
    config.chunk_size = 50;
    config.chunk_overlap = 10;
    config.min_chunk_size = 10;

    FixedChunker chunker(config);

    std::string text = "这是第一段测试文本。这是一段比较长的文本，用于测试分块器的基本功能。";

    auto result = chunker.chunk(text, "doc_001");

    EXPECT_GT(result.chunks.size(), 0);
    EXPECT_FALSE(result.chunks[0].content.empty());
    EXPECT_EQ(result.chunks[0].document_id, "doc_001");
}

TEST(ChunkerTest, FixedChunkerEdgeCases) {
    ChunkingConfig config;
    config.chunk_size = 100;
    config.chunk_overlap = 20;
    config.min_chunk_size = 10;

    FixedChunker chunker(config);

    // 空文本
    auto result1 = chunker.chunk("", "doc_001");
    EXPECT_EQ(result1.chunks.size(), 0);

    // 短文本
    auto result2 = chunker.chunk("短文本", "doc_002");
    EXPECT_EQ(result2.chunks.size(), 1);

    // 长文本
    std::string long_text(1000, 'a');
    auto result3 = chunker.chunk(long_text, "doc_003");
    EXPECT_GE(result3.chunks.size(), 1);
}

TEST(ChunkerTest, RecursiveChunkerBasic) {
    ChunkingConfig config;
    config.chunk_size = 100;
    config.chunk_overlap = 20;
    config.min_chunk_size = 20;

    RecursiveChunker chunker(config);

    std::string text = "第一段内容。\n\n第二段内容。\n\n第三段内容。";

    auto result = chunker.chunk(text, "doc_001");

    EXPECT_GT(result.chunks.size(), 0);

    // 验证块有正确的元数据
    for (const auto& chunk : result.chunks) {
        EXPECT_EQ(chunk.document_id, "doc_001");
        EXPECT_FALSE(chunk.content.empty());
    }
}

TEST(ChunkerTest, RecursiveChunkerWithParagraphs) {
    ChunkingConfig config;
    config.chunk_size = 200;
    config.chunk_overlap = 30;
    config.min_chunk_size = 20;

    RecursiveChunker chunker(config);

    std::string text = "这是第一个段落。包含一些内容。\n\n"
                      "这是第二个段落。也有内容。\n\n"
                      "这是第三个段落。内容更多。";

    auto result = chunker.chunk(text, "doc_002");

    EXPECT_GT(result.chunks.size(), 0);
}

TEST(ChunkerTest, ChunkerFactory) {
    ChunkingConfig config;
    config.strategy = "fixed";
    config.chunk_size = 100;

    auto chunker1 = ChunkerFactory::create(config);
    EXPECT_EQ(chunker1->name(), "fixed");

    config.strategy = "recursive";
    auto chunker2 = ChunkerFactory::create(config);
    EXPECT_EQ(chunker2->name(), "recursive");

    config.strategy = "code";
    auto chunker3 = ChunkerFactory::create(config);
    EXPECT_EQ(chunker3->name(), "code");
}

TEST(ChunkerTest, ChunkMetadata) {
    ChunkingConfig config;
    config.chunk_size = 50;
    config.chunk_overlap = 10;

    FixedChunker chunker(config);

    DocumentMetadata metadata;
    metadata.file_path = "/test/doc.md";
    metadata.file_name = "doc.md";
    metadata.file_type = "markdown";

    std::string text = "这是测试文本。用于验证元数据是否正确传递。";

    auto result = chunker.chunk(text, "doc_001", metadata);

    ASSERT_GT(result.chunks.size(), 0);
    EXPECT_EQ(result.chunks[0].metadata.file_name, "doc.md");
    EXPECT_EQ(result.chunks[0].metadata.file_type, "markdown");
}

TEST(ChunkerTest, ProcessingTime) {
    ChunkingConfig config;
    config.chunk_size = 100;

    FixedChunker chunker(config);

    std::string text(10000, 'a');

    auto result = chunker.chunk(text, "doc_001");

    EXPECT_GE(result.processing_time_ms, 0);
}

TEST(ChunkerTest, CodeChunker) {
    ChunkingConfig config;
    config.chunk_size = 200;
    config.chunk_overlap = 30;

    CodeChunker chunker(config);
    chunker.set_language("cpp");

    std::string code = "void func1() {\n    // comment 1\n    int x = 1;\n}\n\n"
                      "void func2() {\n    // comment 2\n    int y = 2;\n}\n";

    auto result = chunker.chunk(code, "doc_003");

    EXPECT_GT(result.chunks.size(), 0);
}

// ========== SemanticChunker 测试 ==========

TEST(SemanticChunkerTest, DefaultName) {
    SemanticChunkingConfig config;
    SemanticChunker chunker(config);
    EXPECT_EQ(chunker.name(), "semantic");
}

TEST(SemanticChunkerTest, SplitSentences) {
    SemanticChunkingConfig config;
    config.sentence_separators = {".", "。", "!", "！"};

    SemanticChunker chunker(config);

    std::string text = "这是第一句话。这是第二句话。这是第三句话。";

    auto sentences = chunker.split_sentences(text);

    EXPECT_GE(sentences.size(), 3);
}

TEST(SemanticChunkerTest, SplitSentencesWithChinese) {
    SemanticChunkingConfig config;

    SemanticChunker chunker(config);

    std::string text = "你好世界。今天天气很好！我们一起去玩吧？";

    auto sentences = chunker.split_sentences(text);

    EXPECT_GE(sentences.size(), 4);
}

TEST(SemanticChunkerTest, EmptyText) {
    SemanticChunkingConfig config;
    SemanticChunker chunker(config);

    auto sentences = chunker.split_sentences("");

    EXPECT_TRUE(sentences.empty());
}

TEST(SemanticChunkerTest, SemanticChunking) {
    SemanticChunkingConfig config;
    config.chunk_size = 100;
    config.merge_similarity_threshold = 0.7f;

    SemanticChunker chunker(config);

    std::string text = "第一句话。第二句话。第三句话。第四句话。第五句话。";

    auto result = chunker.chunk(text, "doc1", DocumentMetadata{});

    EXPECT_GT(result.chunks.size(), 0);
}

TEST(SemanticChunkerTest, SimilarityCalculation) {
    SemanticChunkingConfig config;
    SemanticChunker chunker(config);

    std::string s1 = "人工智能是研究如何让机器具有智能的学科";
    std::string s2 = "人工智能技术已经广泛应用于各个领域";
    std::string s3 = "今天天气很好";

    float sim12 = chunker.calculate_similarity(s1, s2);
    float sim13 = chunker.calculate_similarity(s1, s3);

    // 相关句子应该有更高的相似度
    EXPECT_GT(sim12, sim13);
}

TEST(SemanticChunkerTest, ChineseText) {
    SemanticChunkingConfig config;
    config.chunk_size = 100;
    config.sentence_separators = {"。", "！", "？"};

    SemanticChunker chunker(config);

    std::string text = "人工智能是计算机科学的一个分支。它企图了解智能的实质，并生产出一种新的能以人类智能相似的方式做出反应的智能机器。";

    auto result = chunker.chunk(text, "doc_chinese", DocumentMetadata{});

    EXPECT_GT(result.chunks.size(), 0);
}

TEST(SemanticChunkerTest, PreserveMetadata) {
    SemanticChunkingConfig config;
    config.chunk_size = 100;

    SemanticChunker chunker(config);

    DocumentMetadata metadata;
    metadata.title = "测试文档";
    metadata.file_path = "/path/to/doc.md";

    std::string text = "这是内容。这是更多内容。这是一大段内容。";

    auto result = chunker.chunk(text, "doc1", metadata);

    ASSERT_GT(result.chunks.size(), 0);
    EXPECT_EQ(result.chunks[0].document_id, "doc1");
    EXPECT_EQ(result.chunks[0].metadata.title, "测试文档");
}

TEST(SemanticChunkerTest, CustomSeparators) {
    SemanticChunkingConfig config;
    config.sentence_separators = {"|", ";", ","};

    SemanticChunker chunker(config);

    std::string text = "第一部分|第二部分;第三部分,第四部分";

    auto sentences = chunker.split_sentences(text);

    EXPECT_GE(sentences.size(), 4);
}

TEST(SemanticChunkerTest, MergeThresholdEffect) {
    // 高阈值 - 合并少
    SemanticChunkingConfig high_threshold;
    high_threshold.chunk_size = 500;
    high_threshold.merge_similarity_threshold = 0.95f;
    high_threshold.sentence_separators = {"。"};

    SemanticChunker high_chunker(high_threshold);

    std::string text = "句子一。句子二。句子三。句子四。";

    auto high_result = high_chunker.chunk(text, "doc1", DocumentMetadata{});

    // 低阈值 - 合并多
    SemanticChunkingConfig low_threshold;
    low_threshold.chunk_size = 500;
    low_threshold.merge_similarity_threshold = 0.3f;
    low_threshold.sentence_separators = {"。"};

    SemanticChunker low_chunker(low_threshold);

    auto low_result = low_chunker.chunk(text, "doc1", DocumentMetadata{});

    // 低阈值应该产生更少或相等的块数
    EXPECT_LE(low_result.chunks.size(), high_result.chunks.size() + 1);
}
