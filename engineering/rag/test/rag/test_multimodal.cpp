#include <rag/multimodal_parser.h>
#include <iostream>
#include <cassert>

using namespace rag;

void test_detect_file_type() {
    assert(MultimodalParser::detect_file_type("test.md") == MultimodalParser::FileType::MARKDOWN);
    assert(MultimodalParser::detect_file_type("test.txt") == MultimodalParser::FileType::TEXT);
    assert(MultimodalParser::detect_file_type("test.png") == MultimodalParser::FileType::IMAGE);
    assert(MultimodalParser::detect_file_type("test.jpg") == MultimodalParser::FileType::IMAGE);
    assert(MultimodalParser::detect_file_type("test.pdf") == MultimodalParser::FileType::PDF);
    assert(MultimodalParser::detect_file_type("test.unknown") == MultimodalParser::FileType::UNKNOWN);
    assert(MultimodalParser::detect_file_type("test") == MultimodalParser::FileType::UNKNOWN);
    std::cout << "DetectFileType: PASSED\n";
}

void test_parse_markdown() {
    MultimodalParser parser;
    MultimodalDocument doc = parser.parse("test.md");
    assert(doc.source_path == "test.md");
    std::cout << "ParseMarkdown: PASSED\n";
}

void test_parse_table() {
    MultimodalParser parser;
    std::vector<std::string> lines = {
        "Name,Age,City",
        "Alice,30,Beijing",
        "Bob,25,Shanghai"
    };
    TableInfo table = parser.parse_table(lines);

    assert(table.headers.size() == 3);
    assert(table.headers[0] == "Name");
    assert(table.headers[1] == "Age");
    assert(table.headers[2] == "City");

    assert(table.rows.size() == 2);
    assert(table.rows[0][0] == "Alice");
    assert(table.rows[1][0] == "Bob");

    assert(table.row_count == 2);
    assert(table.col_count == 3);
    std::cout << "ParseTable: PASSED\n";
}

void test_image_feature_dimension() {
    ImageFeatureExtractor extractor;
    assert(extractor.feature_dimension() == 512);
    std::cout << "ImageFeatureDimension: PASSED\n";
}

void test_extract_features() {
    ImageFeatureExtractor extractor;

    std::vector<float> features1 = extractor.extract_features("image1.png");
    std::vector<float> features2 = extractor.extract_features("image2.png");

    assert(features1.size() == 512);
    assert(features2.size() == 512);

    // 不同图片应有不同特征
    bool different = false;
    for (size_t i = 0; i < features1.size(); ++i) {
        if (std::abs(features1[i] - features2[i]) > 1e-6) {
            different = true;
            break;
        }
    }
    assert(different);

    // 验证特征已归一化 (L2 norm ≈ 1)
    float norm = 0.0f;
    for (size_t i = 0; i < features1.size(); ++i) {
        norm += features1[i] * features1[i];
    }
    norm = std::sqrt(norm);
    assert(std::abs(norm - 1.0f) < 1e-5);

    std::cout << "ExtractFeatures: PASSED\n";
}

void test_parser_enabled() {
    MultimodalParser parser;

    assert(parser.is_enabled() == true);

    parser.set_enabled(false);
    assert(parser.is_enabled() == false);

    parser.set_enabled(true);
    assert(parser.is_enabled() == true);

    std::cout << "ParserEnabled: PASSED\n";
}

int main() {
    test_detect_file_type();
    test_parse_markdown();
    test_parse_table();
    test_image_feature_dimension();
    test_extract_features();
    test_parser_enabled();

    std::cout << "\nAll tests PASSED!\n";
    return 0;
}