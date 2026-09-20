/**
 * @file pdf_parser.cpp
 * @brief PDF 文档解析器实现
 *
 * 实现了基本的 PDF 文本提取功能
 */

#include "mmrag/pdf_parser.h"
#include "mmrag/logger.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cstring>

namespace mmrag {

// ========== 构造函数 ==========

PdfParser::PdfParser() = default;

// ========== 公共接口 ==========

ParseResult PdfParser::parse(const std::filesystem::path& file_path) {
    auto start = std::chrono::steady_clock::now();

    // 读取 PDF 文件
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        throw DocumentException(ErrorCodes::DOCUMENT_NOT_FOUND,
                               "Cannot open PDF file: " + file_path.string());
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string raw_pdf = buffer.str();

    // 提取文本
    std::string content = extract_text_from_pdf(raw_pdf);

    auto result = parse_content(content, file_path.string());

    auto end = std::chrono::steady_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    RAG_INFO("PdfParser: extracted " + std::to_string(result.content.size()) +
             " chars from " + file_path.string());
    return result;
}

ParseResult PdfParser::parse_content(const std::string& content,
                                     const std::string& source) {
    ParseResult result;
    result.content = clean_text(content);

    // 尝试从内容中提取标题（第一行非空文本）
    std::istringstream iss(result.content);
    std::string line;
    while (std::getline(iss, line)) {
        // 去除空白
        size_t start = line.find_first_not_of(" \t\r");
        if (start != std::string::npos) {
            result.title = line.substr(start);
            break;
        }
    }

    return result;
}

// ========== 私有方法 ==========

bool PdfParser::is_valid_pdf(const std::string& content) {
    // PDF 文件必须以 %PDF- 开头
    return content.size() >= 5 &&
           content[0] == '%' && content[1] == 'P' &&
           content[2] == 'D' && content[3] == 'F' && content[4] == '-';
}

std::string PdfParser::extract_text_from_pdf(const std::string& raw_pdf) {
    if (!is_valid_pdf(raw_pdf)) {
        RAG_WARN("Not a valid PDF file, treating as plain text");
        return raw_pdf;
    }

    // 解析 PDF 对象
    auto objects = parse_objects(raw_pdf);

    std::string all_text;

    // 从每个流对象中提取文本
    for (const auto& obj : objects) {
        if (obj.is_stream && !obj.stream_data.empty()) {
            // 尝试解压
            std::string stream_content;
            if (obj.stream_data.size() >= 4 &&
                obj.stream_data.substr(0, 4) == "x\x9c") {
                // FlateDecode 压缩
                stream_content = decompress_flate(obj.stream_data);
            } else {
                stream_content = obj.stream_data;
            }

            // 从内容流中提取文本
            std::string text = extract_text_from_stream(stream_content);
            if (!text.empty()) {
                all_text += text + "\n";
            }
        }
    }

    return all_text;
}

std::vector<PdfParser::PdfObject> PdfParser::parse_objects(const std::string& content) {
    std::vector<PdfObject> objects;

    // 手动查找 PDF 对象（避免 std::regex::dotall 不可用的问题）
    size_t pos = 0;
    while (pos < content.size()) {
        // 查找 "obj" 标记
        size_t obj_start = content.find("obj", pos);
        if (obj_start == std::string::npos) break;

        // 回退查找对象编号（格式：数字 数字 obj）
        size_t line_start = content.rfind('\n', obj_start);
        if (line_start == std::string::npos) line_start = 0;
        else line_start++;

        std::string line = content.substr(line_start, obj_start - line_start + 3);
        int obj_num = 0, gen_num = 0;

        // 尝试解析 "N N obj" 格式
        std::istringstream iss(line);
        if (iss >> obj_num >> gen_num) {
            PdfObject obj;
            obj.obj_num = obj_num;
            obj.gen_num = gen_num;

            // 查找 endobj
            size_t endobj_pos = content.find("endobj", obj_start);
            if (endobj_pos != std::string::npos) {
                std::string body = content.substr(obj_start + 3, endobj_pos - obj_start - 3);

                // 检查是否包含流
                size_t stream_pos = body.find("stream");
                if (stream_pos != std::string::npos) {
                    size_t data_start = body.find('\n', stream_pos);
                    if (data_start == std::string::npos) {
                        data_start = body.find('\r', stream_pos);
                    }
                    if (data_start != std::string::npos) {
                        data_start++;
                        size_t data_end = body.find("endstream", data_start);
                        if (data_end != std::string::npos) {
                            obj.stream_data = body.substr(data_start, data_end - data_start);
                            obj.is_stream = true;

                            // 去除末尾空白
                            while (!obj.stream_data.empty() &&
                                   (obj.stream_data.back() == '\n' ||
                                    obj.stream_data.back() == '\r' ||
                                    obj.stream_data.back() == ' ')) {
                                obj.stream_data.pop_back();
                            }
                        }
                    }
                }

                objects.push_back(obj);
            }
        }

        pos = obj_start + 3;
    }

    return objects;
}

std::string PdfParser::extract_text_from_stream(const std::string& content) {
    std::string result;

    // PDF 文本操作符：
    // Tj - 显示文本字符串
    // TJ - 显示文本数组
    // ' - 移到下一行并显示文本
    // " - 设置间距并显示文本

    // 匹配 Tj 操作符：(text) Tj
    std::regex tj_pattern(R"(\(([^)]*)\)\s*Tj)");
    std::sregex_iterator tj_it(content.begin(), content.end(), tj_pattern);
    std::sregex_iterator tj_end;

    while (tj_it != tj_end) {
        std::string text = (*tj_it)[1].str();
        // 处理转义字符
        std::string cleaned;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\\' && i + 1 < text.size()) {
                switch (text[i + 1]) {
                    case 'n': cleaned += '\n'; i++; break;
                    case 'r': cleaned += '\r'; i++; break;
                    case 't': cleaned += '\t'; i++; break;
                    case '\\': cleaned += '\\'; i++; break;
                    case '(': cleaned += '('; i++; break;
                    case ')': cleaned += ')'; i++; break;
                    default: cleaned += text[i]; break;
                }
            } else {
                cleaned += text[i];
            }
        }
        result += cleaned + " ";
        ++tj_it;
    }

    // 匹配 TJ 操作符：[(text1) (text2) ...] TJ
    std::regex tj_array_pattern(R"(\[(.*?)\]\s*TJ)");
    std::sregex_iterator tja_it(content.begin(), content.end(), tj_array_pattern);
    std::sregex_iterator tja_end;

    while (tja_it != tja_end) {
        std::string array_content = (*tja_it)[1].str();

        // 提取数组中的字符串
        std::regex str_pattern(R"(\(([^)]*)\))");
        std::sregex_iterator str_it(array_content.begin(), array_content.end(), str_pattern);
        std::sregex_iterator str_end;

        while (str_it != str_end) {
            result += (*str_it)[1].str() + " ";
            ++str_it;
        }

        ++tja_it;
    }

    return result;
}

std::string PdfParser::decompress_flate(const std::string& compressed_data) {
    // FlateDecode decompression requires zlib library
    // Without zlib, return raw data (text extraction will be limited)
    RAG_WARN("FlateDecode decompression not available (zlib not linked). "
             "Text from compressed streams may be missing.");
    return compressed_data;
}

std::string PdfParser::clean_text(const std::string& text) {
    std::string result;
    result.reserve(text.size());

    for (char c : text) {
        // 保留可打印字符和基本空白
        if (c == '\n' || c == '\r' || c == '\t') {
            result += c;
        } else if (c >= 32 && c < 127) {
            result += c;
        } else if (c == 0) {
            // 空字节转换为换行
            result += '\n';
        }
        // 其他非打印字符跳过
    }

    // 去除连续空行
    std::string cleaned;
    bool prev_newline = false;
    for (char c : result) {
        if (c == '\n') {
            if (!prev_newline) {
                cleaned += c;
            }
            prev_newline = true;
        } else {
            cleaned += c;
            prev_newline = false;
        }
    }

    return cleaned;
}

}  // namespace mmrag
