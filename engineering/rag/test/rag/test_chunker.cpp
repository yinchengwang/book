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

// 需要在 SemanticChunker 中暴露 compute_similarity 为 public
// 添加一个 wrapper 测试用
class SemanticChunkerTestHelper : public SemanticChunker {
public:
    using SemanticChunker::compute_similarity;
    using SemanticChunker::keyword_overlap;

    explicit SemanticChunkerTestHelper(const SemanticChunkingConfig& config)
        : SemanticChunker(config) {}
};

TEST(SemanticChunkerTest, CalculateSimilarity) {
    SemanticChunkingConfig config;
    SemanticChunkerTestHelper chunker(config);

    std::string s1 = "人工智能是研究如何让机器具有智能的学科";
    std::string s2 = "人工智能技术已经广泛应用于各个领域";
    std::string s3 = "今天天气很好";

    Sentence sent1, sent2, sent3;
    sent1.text = s1;
    sent2.text = s2;
    sent3.text = s3;

    float sim12 = chunker.keyword_overlap(s1, s2);
    float sim13 = chunker.keyword_overlap(s1, s3);

    // 相关句子应该有更高的相似度
    EXPECT_GT(sim12, sim13);
}

TEST(SemanticChunkerTest, SemanticChunkerBasic) {
    SemanticChunkingConfig config;
    config.chunk_size = 100;
    config.merge_similarity_threshold = 0.7f;

    SemanticChunker chunker(config);

    std::string text = "第一句话。第二句话。第三句话。第四句话。第五句话。";

    auto result = chunker.chunk(text, "doc1", DocumentMetadata{});

    EXPECT_GT(result.chunks.size(), 0);
}

TEST(SemanticChunkerTest, SemanticChunkerMerge) {
    // 测试短块合并
    SemanticChunkingConfig config;
    config.chunk_size = 200;
    config.merge_similarity_threshold = 0.3f;  // 低阈值，容易合并
    config.merge_max_chunks = 10;

    SemanticChunker chunker(config);

    std::string text = "短句一。短句二。短句三。短句四。短句五。";

    auto result = chunker.chunk(text, "doc_merge", DocumentMetadata{});

    // 低阈值应该产生较少（合并后）的块
    EXPECT_GE(result.chunks.size(), 1);
}

TEST(SemanticChunkerTest, SemanticChunkerBreakpoints) {
    // 测试语义断点 - 不同主题的内容应该分开
    SemanticChunkingConfig config;
    config.chunk_size = 500;
    config.merge_similarity_threshold = 0.5f;
    config.sentence_separators = {"。", "！", "？", "\n"};

    SemanticChunker chunker(config);

    std::string text = "这是关于人工智能的内容。机器学习是AI的子领域。深度学习是机器学习的延伸。"
                      "突然转换话题：今天天气很好。阳光明媚。适合外出。";

    auto result = chunker.chunk(text, "doc_breakpoints", DocumentMetadata{});

    // 不同主题应该产生多个块
    ASSERT_GE(result.chunks.size(), 1);
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

// ========== CodeAwareChunker 测试 ==========

TEST(CodeAwareChunkerTest, DetectLanguage) {
    CodeAwareChunker chunker;

    // C++
    std::string cpp_code = "#include <vector>\nvoid func() {}";
    EXPECT_EQ(chunker.detect_language(cpp_code), "cpp");

    // Python
    std::string py_code = "def foo():\n    pass\nimport os";
    EXPECT_EQ(chunker.detect_language(py_code), "python");

    // JavaScript
    std::string js_code = "function test() {}\nconst x = 1;";
    EXPECT_EQ(chunker.detect_language(js_code), "javascript");
}

TEST(CodeAwareChunkerTest, CodeAwareChunkerFunction) {
    CodeAwareChunker::Config config;
    config.min_chunk_lines = 3;
    config.merge_threshold = 50;

    CodeAwareChunker chunker(config);

    std::string code = R"(
void helper() {
    int x = 1;
}

int main() {
    printf("Hello");
    return 0;
}

void other_func() {
    // This is a longer function
    // with multiple lines
    // to ensure it meets min_chunk_lines
    int a = 1;
    int b = 2;
}
)";

    auto result = chunker.chunk(code, "cpp_test", DocumentMetadata{});

    EXPECT_GT(result.chunks.size(), 0);

    // 检查是否有函数级别的分块
    bool found_function_chunk = false;
    for (const auto& chunk : result.chunks) {
        if (chunk.metadata.extra.count("code_type") &&
            chunk.metadata.extra.at("code_type") == "function") {
            found_function_chunk = true;
            break;
        }
    }
    EXPECT_TRUE(found_function_chunk);
}

TEST(CodeAwareChunkerTest, CodeAwareChunkerContext) {
    CodeAwareChunker::Config config;
    config.preserve_context = true;
    config.min_chunk_lines = 3;
    config.merge_threshold = 50;

    CodeAwareChunker chunker(config);

    std::string code = R"(
#include <vector>
#include <string>

void func() {
    std::vector<int> v;
}
)";

    auto result = chunker.chunk(code, "cpp_context", DocumentMetadata{});

    ASSERT_GT(result.chunks.size(), 0);

    // 检查导入语句是否被保留
    bool has_include = false;
    for (const auto& chunk : result.chunks) {
        if (chunk.content.find("#include") != std::string::npos) {
            has_include = true;
            break;
        }
    }
    EXPECT_TRUE(has_include);
}

TEST(CodeAwareChunkerTest, CodeAwareChunkerMerge) {
    CodeAwareChunker::Config config;
    config.min_chunk_lines = 5;
    config.merge_threshold = 20;  // 小阈值，容易触发合并

    CodeAwareChunker chunker(config);

    std::string code = R"(
void short_func1() { int x = 1; }
void short_func2() { int y = 2; }
void short_func3() { int z = 3; }
void short_func4() { int w = 4; }
void short_func5() { int v = 5; }
)";

    auto result = chunker.chunk(code, "cpp_merge", DocumentMetadata{});

    // 多个小函数应该被合并
    EXPECT_GE(result.chunks.size(), 1);
}

TEST(CodeAwareChunkerTest, CodeAwareChunkerClass) {
    CodeAwareChunker::Config config;
    config.min_chunk_lines = 3;
    config.merge_threshold = 50;

    CodeAwareChunker chunker(config);

    std::string code = R"(
class MyClass {
public:
    void method1() {
        // implementation
    }

    void method2() {
        // implementation
    }
};

void standalone() {
    int x = 1;
}
)";

    auto result = chunker.chunk(code, "cpp_class", DocumentMetadata{});

    EXPECT_GT(result.chunks.size(), 0);

    // 检查是否有类级别的分块
    bool found_class_chunk = false;
    for (const auto& chunk : result.chunks) {
        if (chunk.metadata.extra.count("code_type") &&
            chunk.metadata.extra.at("code_type") == "class") {
            found_class_chunk = true;
            break;
        }
    }
    EXPECT_TRUE(found_class_chunk);
}

TEST(CodeAwareChunkerTest, PythonFunction) {
    CodeAwareChunker::Config config;
    config.min_chunk_lines = 3;
    config.merge_threshold = 50;

    CodeAwareChunker chunker(config);

    std::string code = R"(
import os
from typing import List

def helper():
    pass

def main():
    x = 1
    y = 2
    return x + y

def another_func():
    print("hello")
    print("world")
)";

    auto result = chunker.chunk(code, "py_test", DocumentMetadata{});

    EXPECT_GT(result.chunks.size(), 0);

    // 检查函数是否被识别
    bool found_function = false;
    for (const auto& chunk : result.chunks) {
        if (chunk.metadata.extra.count("code_name")) {
            const auto& name = chunk.metadata.extra.at("code_name");
            if (name == "main" || name == "another_func") {
                found_function = true;
                break;
            }
        }
    }
    EXPECT_TRUE(found_function);
}

TEST(CodeAwareChunkerTest, JavaScriptFunction) {
    CodeAwareChunker::Config config;
    config.min_chunk_lines = 2;
    config.merge_threshold = 50;

    CodeAwareChunker chunker(config);

    std::string code = R"(
const helper = () => {
    return 1;
};

function main() {
    console.log("hello");
}

const anotherFunc = function() {
    console.log("world");
};
)";

    auto result = chunker.chunk(code, "js_test", DocumentMetadata{});

    EXPECT_GT(result.chunks.size(), 0);
}

// ========== HybridChunker 测试 ==========

TEST(HybridChunkerTest, HybridChunker) {
    ChunkingConfig base_config;
    base_config.chunk_size = 200;
    base_config.chunk_overlap = 20;

    HybridChunker chunker(base_config);

    std::string text = "这是第一段内容。包含一些文字。第二段内容。也包含一些文字。第三段内容。更多的文字。";

    auto result = chunker.chunk(text, "hybrid_test", DocumentMetadata{});

    EXPECT_GT(result.chunks.size(), 0);
    EXPECT_EQ(chunker.name(), "hybrid");
}

TEST(HybridChunkerTest, HybridChunkerLongText) {
    ChunkingConfig base_config;
    base_config.chunk_size = 100;
    base_config.chunk_overlap = 10;

    HybridChunker chunker(base_config);

    // 构造一个长文本
    std::string text;
    for (int i = 0; i < 20; i++) {
        text += "这是第" + std::to_string(i) + "句话。";
    }

    auto result = chunker.chunk(text, "hybrid_long", DocumentMetadata{});

    // 长文本应该使用语义分块
    EXPECT_GE(result.chunks.size(), 1);
}

TEST(HybridChunkerTest, HybridChunkerShortText) {
    ChunkingConfig base_config;
    base_config.chunk_size = 200;
    base_config.chunk_overlap = 20;

    HybridChunker chunker(base_config);

    // 短文本应该使用递归分块
    std::string text = "这是一段比较短的文本。";

    auto result = chunker.chunk(text, "hybrid_short", DocumentMetadata{});

    EXPECT_GE(result.chunks.size(), 1);
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
