/**
 * @file test_parser.cpp
 * @brief 解析器测试
 */

#include <gtest/gtest.h>
#include "rag/parser.h"
#include <fstream>
#include <filesystem>

using namespace rag;

class ParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建临时测试文件
        temp_dir_ = std::filesystem::temp_directory_path() / "rag_test";
        std::filesystem::create_directory(temp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(temp_dir_);
    }

    std::filesystem::path temp_dir_;
};

TEST_F(ParserTest, MarkdownParser) {
    std::string content = R"(
# 标题

这是正文内容。

## 子标题

更多内容。

```cpp
int x = 1;
```
)";

    MarkdownParser parser;
    auto result = parser.parse_content(content);

    EXPECT_FALSE(result.content.empty());
    EXPECT_EQ(result.title, "标题");
}

TEST_F(ParserTest, MarkdownParserSupportedExtensions) {
    MarkdownParser parser;
    auto exts = parser.supported_extensions();

    EXPECT_NE(std::find(exts.begin(), exts.end(), ".md"), exts.end());
    EXPECT_NE(std::find(exts.begin(), exts.end(), ".markdown"), exts.end());
}

TEST_F(ParserTest, TextParser) {
    std::string content = "Plain text content.";

    TextParser parser;
    auto result = parser.parse_content(content);

    EXPECT_EQ(result.content, content);
}

TEST_F(ParserTest, TextParserSupportedExtensions) {
    TextParser parser;
    auto exts = parser.supported_extensions();

    EXPECT_NE(std::find(exts.begin(), exts.end(), ".txt"), exts.end());
}

TEST_F(ParserTest, CodeParser) {
    std::string content = R"(
/**
 * @brief A function
 */
int add(int a, int b) {
    return a + b;  // return sum
}
)";

    CodeParser parser;
    auto result = parser.parse_content(content, "test.cpp");

    EXPECT_FALSE(result.content.empty());
}

TEST_F(ParserTest, CodeParserSupportedExtensions) {
    CodeParser parser;
    auto exts = parser.supported_extensions();

    EXPECT_NE(std::find(exts.begin(), exts.end(), ".cpp"), exts.end());
    EXPECT_NE(std::find(exts.begin(), exts.end(), ".h"), exts.end());
    EXPECT_NE(std::find(exts.begin(), exts.end(), ".py"), exts.end());
}

TEST_F(ParserTest, ParserRegistry) {
    ParserRegistry registry;

    auto* md_parser = registry.get_parser(std::filesystem::path("test.md"));
    EXPECT_EQ(md_parser->name(), "markdown");

    auto* cpp_parser = registry.get_parser(std::filesystem::path("test.cpp"));
    EXPECT_EQ(cpp_parser->name(), "code");

    auto* txt_parser = registry.get_parser(std::filesystem::path("test.txt"));
    EXPECT_EQ(txt_parser->name(), "text");
}

TEST_F(ParserTest, DetectFileType) {
    EXPECT_EQ(detect_file_type("test.md"), "markdown");
    EXPECT_EQ(detect_file_type("test.cpp"), "cpp");
    EXPECT_EQ(detect_file_type("test.py"), "python");
    EXPECT_EQ(detect_file_type("test.java"), "java");
    EXPECT_EQ(detect_file_type("test.js"), "javascript");
    EXPECT_EQ(detect_file_type("test.txt"), "text");
    EXPECT_EQ(detect_file_type("test.unknown"), "unknown");
}

TEST_F(ParserTest, GetFileMetadata) {
    auto file_path = temp_dir_ / "test.md";
    std::ofstream(file_path) << "# Test";

    auto metadata = get_file_metadata(file_path);

    EXPECT_EQ(metadata.file_name, "test.md");
    EXPECT_EQ(metadata.file_type, "markdown");
    EXPECT_GT(metadata.file_size, 0);
}
