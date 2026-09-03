#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace rag {

// ========== 多模态元素 ==========

struct ImageInfo {
    std::string path;
    std::string extracted_text;
    int width = 0;
    int height = 0;
    std::string format;
};

struct TableInfo {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> headers;
    int row_count = 0;
    int col_count = 0;
};

struct CodeBlock {
    std::string code;
    std::string language;
    int start_line = 0;
    int end_line = 0;
};

// ========== 多模态文档 ==========

struct MultimodalDocument {
    std::string text;
    std::vector<ImageInfo> images;
    std::vector<TableInfo> tables;
    std::vector<CodeBlock> code_blocks;
    std::string source_path;
    int page_count = 0;
};

// ========== 多模态解析器 ==========

class MultimodalParser {
public:
    MultimodalParser();

    // 解析文档
    MultimodalDocument parse(const std::string& path);

    // 解析图片（OCR）
    std::string extract_text_from_image(const std::string& image_path);

    // 解析表格
    TableInfo parse_table(const std::vector<std::string>& lines);

    // 解析 PDF
    MultimodalDocument parse_pdf(const std::string& pdf_path);

    // 检测文件类型
    enum class FileType { UNKNOWN, TEXT, IMAGE, PDF, MARKDOWN };
    static FileType detect_file_type(const std::string& path);

    // 是否启用
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

private:
    bool enabled_ = true;
};

// ========== 图片特征提取器 ==========

class ImageFeatureExtractor {
public:
    ImageFeatureExtractor();

    // 提取图片特征向量
    std::vector<float> extract_features(const std::string& image_path);

    // 批量提取
    std::vector<std::vector<float>> extract_batch(const std::vector<std::string>& images);

    // 特征维度
    int feature_dimension() const { return 512; }

private:
    // 简化的颜色直方图特征
    std::vector<float> extract_histogram_features(const std::string& image_path);
};

// ========== 工厂函数 ==========

std::unique_ptr<MultimodalParser> create_multimodal_parser();
std::unique_ptr<ImageFeatureExtractor> create_image_feature_extractor();

}  // namespace rag