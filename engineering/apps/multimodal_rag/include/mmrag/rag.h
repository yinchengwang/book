/**
 * @file rag.h
 * @brief RAG 系统主头文件
 *
 * 包含所有公共接口的汇总导出
 */

#pragma once

// 核心类型
#include "mmrag/error.h"
#include "mmrag/retry.h"
#include "mmrag/types.h"
#include "mmrag/config.h"
#include "mmrag/database.h"
#include "mmrag/logger.h"

// 功能模块
#include "mmrag/chunker.h"
#include "mmrag/parser.h"
#include "mmrag/vector_index.h"
#include "mmrag/bm25_index.h"
#include "mmrag/retriever.h"
#include "mmrag/embedding.h"
#include "mmrag/reranker.h"
#include "mmrag/enhanced_retriever.h"
#include "mmrag/engine.h"
#include "mmrag/cli.h"

namespace mmrag {

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

}  // namespace mmrag
