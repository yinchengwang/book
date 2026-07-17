/**
 * @file error.h
 * @brief RAG 系统错误处理
 */
#pragma once

#include <string>
#include <stdexcept>
#include <unordered_map>
#include <memory>

namespace rag {

// ========== ErrorCode 结构 ==========

struct ErrorCode {
    std::string code;
    int http_status;
    std::string category;
    std::string message;

    ErrorCode() : code(""), http_status(500), category(""), message("") {}
    ErrorCode(const std::string& c, int status, const std::string& cat, const std::string& msg)
        : code(c), http_status(status), category(cat), message(msg) {}

    explicit operator bool() const { return !code.empty(); }
};

// 预定义错误码
namespace errors {

const ErrorCode SUCCESS("", 200, "", "Success");

const ErrorCode INVALID_ARGUMENT("INVALID_ARGUMENT", 400, "client",
    "Invalid argument provided");

const ErrorCode NOT_FOUND("NOT_FOUND", 404, "client",
    "The requested resource was not found");

const ErrorCode ALREADY_EXISTS("ALREADY_EXISTS", 409, "client",
    "Resource already exists");

const ErrorCode INTERNAL_ERROR("INTERNAL_ERROR", 500, "server",
    "An internal error occurred");

const ErrorCode NOT_INITIALIZED("NOT_INITIALIZED", 500, "server",
    "Component not initialized");

const ErrorCode ALREADY_INITIALIZED("ALREADY_INITIALIZED", 500, "server",
    "Component already initialized");

const ErrorCode IO_ERROR("IO_ERROR", 500, "server",
    "I/O error occurred");

const ErrorCode PARSE_ERROR("PARSE_ERROR", 400, "client",
    "Failed to parse input");

const ErrorCode INDEX_ERROR("INDEX_ERROR", 500, "server",
    "Index operation failed");

const ErrorCode CONFIG_ERROR("CONFIG_ERROR", 500, "server",
    "Configuration error");

}  // namespace errors

// 别名，用于兼容旧代码
namespace ErrorCodes {
    // 兼容旧的 ErrorCode 引用方式
    inline const ErrorCode& DOCUMENT_NOT_FOUND = errors::NOT_FOUND;
    inline const ErrorCode& PARSE_ERROR = errors::PARSE_ERROR;
    inline const ErrorCode& INVALID_ARGUMENT = errors::INVALID_ARGUMENT;
    inline const ErrorCode& INTERNAL_ERROR = errors::INTERNAL_ERROR;
    inline const ErrorCode& NOT_FOUND = errors::NOT_FOUND;
    inline const ErrorCode& ALREADY_EXISTS = errors::ALREADY_EXISTS;
    inline const ErrorCode& IO_ERROR = errors::IO_ERROR;
    inline const ErrorCode& INDEX_ERROR = errors::INDEX_ERROR;
    inline const ErrorCode& CONFIG_ERROR = errors::CONFIG_ERROR;
    inline const ErrorCode& LLM_NOT_AVAILABLE = errors::NOT_FOUND;
    inline const ErrorCode& MODEL_NOT_FOUND = errors::NOT_FOUND;
    inline const ErrorCode& CONFIG_NOT_FOUND = errors::NOT_FOUND;
    inline const ErrorCode& CONFIG_INVALID = errors::CONFIG_ERROR;
    inline const ErrorCode& INDEX_BUILD_FAILED = errors::INDEX_ERROR;
    inline const ErrorCode& INDEX_NOT_FOUND = errors::NOT_FOUND;
    inline const ErrorCode& INDEX_CORRUPTED = errors::INDEX_ERROR;
}

// ========== RAGException 类 ==========

class RAGException : public std::runtime_error {
public:
    // 无参构造函数
    RAGException() : std::runtime_error("Unknown error"), code_(""), http_status_(500),
                      message_("Unknown error") {}

    RAGException(const ErrorCode& code,
                 const std::string& message = "",
                 const std::string& details = "");

    RAGException(const std::string& code,
                 int http_status,
                 const std::string& category,
                 const std::string& message,
                 const std::string& details = "");

    const char* what() const noexcept override;

    // 获取错误码
    std::string code() const { return code_; }

    // 克隆函数
    virtual std::unique_ptr<RAGException> clone() const {
        return std::make_unique<RAGException>(*this);
    }

    const std::string& get_code() const { return code_; }
    int get_http_status() const { return http_status_; }
    const std::string& get_category() const { return category_; }
    const std::string& get_message() const { return message_; }
    const std::string& get_details() const { return details_; }

    void set_context(const std::string& key, const std::string& value);

    std::string to_json() const;

private:
    std::string code_;
    int http_status_ = 500;
    std::string category_;
    std::string message_;
    std::string details_;
    mutable std::string what_buffer_;
    std::unordered_map<std::string, std::string> context_;
};

// ========== 文档解析异常 ==========

class DocumentException : public RAGException {
public:
    using RAGException::RAGException;
};

// ========== LLM 异常 ==========

class LLMException : public RAGException {
public:
    using RAGException::RAGException;
};

// ========== Config 异常 ==========

class ConfigException : public RAGException {
public:
    using RAGException::RAGException;
};

// ========== Index 异常 ==========

class IndexException : public RAGException {
public:
    using RAGException::RAGException;
};

}  // namespace rag
