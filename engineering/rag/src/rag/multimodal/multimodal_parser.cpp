#include <rag/multimodal_parser.h>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace rag {

// ========== MultimodalParser ==========

MultimodalParser::MultimodalParser() = default;

MultimodalDocument MultimodalParser::parse(const std::string& path) {
    MultimodalDocument doc;
    doc.source_path = path;

    FileType type = detect_file_type(path);

    switch (type) {
        case FileType::MARKDOWN:
        case FileType::TEXT: {
            std::ifstream file(path);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                doc.text = buffer.str();
            }
            break;
        }
        case FileType::IMAGE: {
            ImageInfo img;
            img.path = path;
            img.extracted_text = extract_text_from_image(path);
            doc.images.push_back(img);
            break;
        }
        case FileType::PDF: {
            return parse_pdf(path);
        }
        default:
            break;
    }

    return doc;
}

std::string MultimodalParser::extract_text_from_image(const std::string& image_path) {
    // Mock OCR - 返回占位文本
    return "[OCR extracted text from: " + image_path + "]";
}

TableInfo MultimodalParser::parse_table(const std::vector<std::string>& lines) {
    TableInfo table;

    if (lines.empty()) {
        return table;
    }

    // CSV 格式表格解析
    for (size_t i = 0; i < lines.size(); ++i) {
        std::vector<std::string> row;
        std::stringstream ss(lines[i]);
        std::string cell;

        while (std::getline(ss, cell, ',')) {
            // 移除引号
            if (!cell.empty() && cell.front() == '"' && cell.back() == '"') {
                cell = cell.substr(1, cell.length() - 2);
            }
            row.push_back(cell);
        }

        if (i == 0) {
            table.headers = row;
            table.col_count = static_cast<int>(row.size());
        } else {
            table.rows.push_back(row);
        }
    }

    table.row_count = static_cast<int>(table.rows.size());
    return table;
}

MultimodalDocument MultimodalParser::parse_pdf(const std::string& pdf_path) {
    MultimodalDocument doc;
    doc.source_path = pdf_path;
    doc.page_count = 1;  // Mock

    // Mock PDF 解析
    doc.text = "[PDF content from: " + pdf_path + "]";
    return doc;
}

MultimodalParser::FileType MultimodalParser::detect_file_type(const std::string& path) {
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return FileType::UNKNOWN;
    }

    std::string ext = path.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "md" || ext == "markdown") {
        return FileType::MARKDOWN;
    } else if (ext == "txt" || ext == "text" || ext == "log") {
        return FileType::TEXT;
    } else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || ext == "bmp") {
        return FileType::IMAGE;
    } else if (ext == "pdf") {
        return FileType::PDF;
    }

    return FileType::UNKNOWN;
}

// ========== ImageFeatureExtractor ==========

ImageFeatureExtractor::ImageFeatureExtractor() = default;

std::vector<float> ImageFeatureExtractor::extract_features(const std::string& image_path) {
    return extract_histogram_features(image_path);
}

std::vector<std::vector<float>> ImageFeatureExtractor::extract_batch(const std::vector<std::string>& images) {
    std::vector<std::vector<float>> features;
    features.reserve(images.size());
    for (const auto& img : images) {
        features.push_back(extract_features(img));
    }
    return features;
}

std::vector<float> ImageFeatureExtractor::extract_histogram_features(const std::string& image_path) {
    // 简化的颜色直方图特征 - 512维
    std::vector<float> features(512, 0.0f);

    // 基于路径生成伪随机但确定的特征
    // 实际应用中应读取图片文件并计算真实颜色直方图
    float sum = 0.0f;
    for (size_t i = 0; i < image_path.size(); ++i) {
        features[i % 512] += static_cast<float>(image_path[i]) / 255.0f;
        sum += features[i % 512];
    }

    // L2 归一化
    if (sum > 0.0f) {
        float norm = 0.0f;
        for (size_t i = 0; i < features.size(); ++i) {
            features[i] /= sum;
            norm += features[i] * features[i];
        }
        norm = std::sqrt(norm);
        if (norm > 0.0f) {
            for (size_t i = 0; i < features.size(); ++i) {
                features[i] /= norm;
            }
        }
    }

    return features;
}

// ========== 工厂函数 ==========

std::unique_ptr<MultimodalParser> create_multimodal_parser() {
    return std::make_unique<MultimodalParser>();
}

std::unique_ptr<ImageFeatureExtractor> create_image_feature_extractor() {
    return std::make_unique<ImageFeatureExtractor>();
}

}  // namespace rag