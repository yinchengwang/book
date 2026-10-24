/**
 * @file vectors_sql.c
 * @brief 向量模型专用 SQL 辅助（P2 扩展点）
 *
 * 当前 vectors.c 内联了所有 SQL，未来可在此拆分 DDL/查询构造逻辑。
 */
#include "sdk/mmdb.h"

int mmdb_vectors_sql_dummy(void) { return 0; }