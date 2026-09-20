/**
 * @file pdf_parser.h
 * @brief PDF 文档解析器 - 从 PDF 文件中提取文本内容
 *
 * 实现了基本的 PDF 文本提取，支持：
 * - 解析 PDF 对象结构
 * - 提取文本流内容
 * - 处理文本操作符 (Tj, TJ, ', ")
 * - 过滤二进制数据
 */

#pragma once

#include "mmrag/parser.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace mmrag {

/**
 * @brief PDF 解析器
 *
 * 从 PDF 文件中提取可读文本内容
 */
class PdfParser : public Parser {
public:
    PdfParser();

    ParseResult parse(const std::filesystem::path& file_path) override;
    ParseResult parse_content(const std::string& content,
                              const std::string& source = "") override;

    std::vector<std::string> supported_extensions() const override {
        return {".pdf"};
    }

    std::string name() const override { return "pdf"; }

private:
    // PDF 对象结构
    struct PdfObject {
        int obj_num = 0;
        int gen_num = 0;
        std::string stream_data;
        bool is_stream = false;
    };

    // 从原始 PDF 内容中提取文本
    std::string extract_text_from_pdf(const std::string& raw_pdf);

    // 解析 PDF 对象
    std::vector<PdfObject> parse_objects(const std::string& content);

    // 从内容流中提取文本
    std::string extract_text_from_stream(const std::string& stream);

    // 解码 FlateDecode 压缩的数据
    std::string decompress_flate(const std::string& compressed_data);

    // 提取文本操作符中的字符串
    std::string extract_text_operators(const std::string& content);

    // 检查是否是有效的 PDF 文件
    bool is_valid_pdf(const std::string& content);

    // 清理提取的文本
    std::string clean_text(const std::string& text);
};

}  // namespace mmrag
