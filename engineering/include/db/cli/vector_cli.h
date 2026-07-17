/**
 * @file vector_cli.h
 * @brief 向量 CLI 命令头文件
 *
 * 提供向量集合管理的交互式 CLI 命令。
 *
 * 支持的命令:
 * - create collection <name> <dim> [--index hnsw|diskann|ivf] [--metric l2|cosine|ip]
 * - list collections
 * - drop collection <name>
 * - insert vectors <collection> <file.json>
 * - search vectors <collection> <query_vector> <top_k>
 * - delete vectors <collection> <id1,id2,...>
 * - collection info <name>
 * - save
 * - load
 */
#ifndef DB_VECTOR_CLI_H
#define DB_VECTOR_CLI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CLI 上下文
 * ======================================================================== */

/** 向量 CLI 上下文（不透明） */
typedef struct VectorCLI_s VectorCLI;

/* ========================================================================
 * CLI 生命周期
 * ======================================================================== */

/**
 * @brief 创建向量 CLI
 * @param api 向量 API 句柄
 * @return CLI 句柄
 */
VectorCLI *vector_cli_create(void *api);

/**
 * @brief 销毁向量 CLI
 * @param cli CLI 句柄
 */
void vector_cli_destroy(VectorCLI *cli);

/* ========================================================================
 * 命令处理
 * ======================================================================== */

/**
 * @brief 处理向量 CLI 命令
 * @param cli CLI 句柄
 * @param cmd 命令字符串
 * @return 0 成功，负数失败
 */
int vector_cli_exec(VectorCLI *cli, const char *cmd);

/**
 * @brief 获取帮助信息
 * @return 帮助文本
 */
const char *vector_cli_help(void);

/* ========================================================================
 * 交互式模式
 * ======================================================================== */

/**
 * @brief 运行交互式向量 CLI
 * @param cli CLI 句柄
 * @return 0 正常退出
 */
int vector_cli_run(VectorCLI *cli);

/* ========================================================================
 * JSON 工具
 * ======================================================================== */

/**
 * @brief 解析向量 JSON 文件
 * @param filename JSON 文件名
 * @param out_ids 输出 ID 数组（可选）
 * @param out_vectors 输出向量数组
 * @param out_n 输出向量数量
 * @param out_dim 输出向量维度
 * @return 0 成功，负数失败
 */
int vector_cli_parse_vectors_json(const char *filename,
                                   int64_t **out_ids,
                                   float ***out_vectors,
                                   int32_t *out_n,
                                   int32_t *out_dim);

/**
 * @brief 格式化搜索结果为 JSON
 * @param results 搜索结果
 * @param with_distance 是否包含距离
 * @return JSON 字符串（调用者负责释放）
 */
char *vector_cli_format_results_json(const void *results,
                                      bool with_distance);

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTOR_CLI_H */
