/**
 * @file error.cpp
 * @brief RAG 系统错误处理实现
 */

#include "rag/error.h"
#include <sstream>
#include <iomanip>

namespace rag {

// ========== RAGException 实现 ==========

RAGException::RAGException(const ErrorCode& code,
                           const std::string& message,
                           const std::string& details)
    : std::runtime_error(message.empty() ? code.message : message),
      code_(code.code),
      http_status_(code.http_status),
      category_(code.category),
      message_(message.empty() ? code.message : message),
      details_(details) {}

RAGException::RAGException(const std::string& code,
                           int http_status,
                           const std::string& category,
                           const std::string& message,
                           const std::string& details)
    : std::runtime_error(message),
      code_(code),
      http_status_(http_status),
      category_(category),
      message_(message),
      details_(details) {}

const char* RAGException::what() const noexcept {
    what_buffer_ = code_ + ": " + message_;
    if (!details_.empty()) {
        what_buffer_ += " (" + details_ + ")";
    }
    return what_buffer_.c_str();
}

void RAGException::set_context(const std::string& key, const std::string& value) {
    context_[key] = value;
}

std::string RAGException::to_json() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"error\": {\n";
    oss << "    \"code\": \"" << code_ << "\",\n";
    oss << "    \"message\": \"" << message_ << "\",\n";
    oss << "    \"details\": \"" << details_ << "\",\n";
    oss << "    \"category\": \"" << category_ << "\",\n";
    oss << "    \"http_status\": " << http_status_ << "\n";
    oss << "  },\n";

    if (!context_.empty()) {
        oss << "  \"context\": {\n";
        bool first = true;
        for (const auto& [key, value] : context_) {
            if (!first) oss << ",\n";
            oss << "    \"" << key << "\": \"" << value << "\"";
            first = false;
        }
        oss << "\n  },\n";
    }

    oss << "  \"request_id\": \"\"\n";
    oss << "}";
    return oss.str();
}

}  // namespace rag
