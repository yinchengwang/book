/*
 * rag_err.h - RAG 系统错误码
 *
 * 错误码命名规范: RAG_<级别>_<描述>
 * 级别: WARN / ERROR / FATAL
 */
#ifndef RAG_RAG_ERR_H
#define RAG_RAG_ERR_H

#include "common/base_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 系统错误 (100)
// ============================================================

#define RAG_ERROR_INTERNAL                   "RAG_ERROR_INTERNAL"           // 内部错误
#define RAG_ERROR_NOT_IMPLEMENTED            "RAG_ERROR_NOT_IMPLEMENTED"   // 功能未实现
#define RAG_ERROR_TIMEOUT                    "RAG_ERROR_TIMEOUT"           // 操作超时
#define RAG_ERROR_RESOURCE_EXHAUSTED         "RAG_ERROR_RESOURCE_EXHAUSTED" // 资源耗尽

// ============================================================
// 配置错误 (200)
// ============================================================

#define RAG_ERROR_CONFIG_NOT_FOUND           "RAG_ERROR_CONFIG_NOT_FOUND"  // 配置文件未找到
#define RAG_ERROR_CONFIG_INVALID             "RAG_ERROR_CONFIG_INVALID"    // 配置无效
#define RAG_ERROR_CONFIG_MISSING_FIELD       "RAG_ERROR_CONFIG_MISSING_FIELD" // 缺少必需字段

// ============================================================
// 模型错误 (300)
// ============================================================

#define RAG_ERROR_MODEL_NOT_FOUND            "RAG_ERROR_MODEL_NOT_FOUND"   // 模型文件未找到
#define RAG_ERROR_MODEL_LOAD_FAILED          "RAG_ERROR_MODEL_LOAD_FAILED" // 模型加载失败
#define RAG_ERROR_MODEL_UNSUPPORTED          "RAG_ERROR_MODEL_UNSUPPORTED" // 不支持的模型
#define RAG_ERROR_MODEL_INFERENCE_FAILED     "RAG_ERROR_MODEL_INFERENCE_FAILED" // 推理失败
#define RAG_ERROR_MODEL_OUT_OF_MEMORY        "RAG_ERROR_MODEL_OUT_OF_MEMORY" // 内存不足

// ============================================================
// 索引错误 (400)
// ============================================================

#define RAG_ERROR_INDEX_NOT_FOUND            "RAG_ERROR_INDEX_NOT_FOUND"   // 索引未找到
#define RAG_ERROR_INDEX_NOT_READY            "RAG_ERROR_INDEX_NOT_READY"   // 索引未就绪
#define RAG_ERROR_INDEX_BUILD_FAILED         "RAG_ERROR_INDEX_BUILD_FAILED" // 索引构建失败
#define RAG_ERROR_INDEX_CORRUPTED            "RAG_ERROR_INDEX_CORRUPTED"   // 索引损坏

// ============================================================
// 文档错误 (500)
// ============================================================

#define RAG_ERROR_DOCUMENT_NOT_FOUND         "RAG_ERROR_DOCUMENT_NOT_FOUND" // 文档未找到
#define RAG_ERROR_DOCUMENT_TOO_LARGE         "RAG_ERROR_DOCUMENT_TOO_LARGE" // 文档过大
#define RAG_ERROR_UNSUPPORTED_FILE_TYPE      "RAG_ERROR_UNSUPPORTED_FILE_TYPE" // 不支持的文件类型
#define RAG_ERROR_DOCUMENT_PARSE_FAILED      "RAG_ERROR_DOCUMENT_PARSE_FAILED" // 文档解析失败

// ============================================================
// 检索错误 (600)
// ============================================================

#define RAG_ERROR_RETRIEVAL_FAILED           "RAG_ERROR_RETRIEVAL_FAILED"  // 检索失败
#define RAG_ERROR_EMPTY_RESULT               "RAG_ERROR_EMPTY_RESULT"      // 结果为空
#define RAG_ERROR_QUERY_PARSE_FAILED         "RAG_ERROR_QUERY_PARSE_FAILED" // 查询解析失败

// ============================================================
// LLM 错误 (700)
// ============================================================

#define RAG_ERROR_LLM_NOT_AVAILABLE          "RAG_ERROR_LLM_NOT_AVAILABLE" // LLM 服务不可用
#define RAG_ERROR_LLM_GENERATION_FAILED      "RAG_ERROR_LLM_GENERATION_FAILED" // 生成失败
#define RAG_ERROR_LLM_TIMEOUT                "RAG_ERROR_LLM_TIMEOUT"       // 生成超时
#define RAG_ERROR_LLM_CONTENT_FILTERED       "RAG_ERROR_LLM_CONTENT_FILTERED" // 内容被过滤

// ============================================================
// 用户错误 (900)
// ============================================================

#define RAG_ERROR_INVALID_REQUEST            "RAG_ERROR_INVALID_REQUEST"   // 无效请求
#define RAG_ERROR_UNAUTHORIZED               "RAG_ERROR_UNAUTHORIZED"      // 未授权
#define RAG_ERROR_RATE_LIMITED               "RAG_ERROR_RATE_LIMITED"      // 速率限制

// ============================================================
// 警告级别错误码
// ============================================================

#define RAG_WARN_QUERY_TOO_SHORT             "RAG_WARN_QUERY_TOO_SHORT"   // 查询过短
#define RAG_WARN_QUERY_TOO_LONG              "RAG_WARN_QUERY_TOO_LONG"    // 查询过长
#define RAG_WARN_RESULT_TRUNCATED            "RAG_WARN_RESULT_TRUNCATED"  // 结果被截断
#define RAG_WARN_DEPRECATED_API              "RAG_WARN_DEPRECATED_API"    // 使用了废弃 API

#ifdef __cplusplus
}
#endif

#endif // RAG_RAG_ERR_H
