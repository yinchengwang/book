/**
 * @file rag.h
 * @brief RAG 系统主头文件
 *
 * 包含所有公共接口的汇总导出
 */

#pragma once

// 核心类型
#include "rag/error.h"
#include "rag/retry.h"
#include "rag/types.h"
#include "rag/config.h"
#include "rag/database.h"
#include "rag/logger.h"

// 功能模块
#include "rag/chunker.h"
#include "rag/parser.h"
#include "rag/vector_index.h"
#include "rag/bm25_index.h"
#include "rag/retriever.h"
#include "rag/embedding.h"
#include "rag/reranker.h"
#include "rag/enhanced_retriever.h"
#include "rag/engine.h"
#include "rag/cli.h"

namespace rag {

/**
 * @brief 获取 RAG 版本
 */
inline const char* version() {
    return "1.0.0";
}

/**
 * @brief 获取构建信息
 */
inline const char* build_info() {
    return "RAG System v1.0.0 - Built " __DATE__ " " __TIME__;
}

}  // namespace rag
