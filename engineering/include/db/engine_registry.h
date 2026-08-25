/**
 * @file engine_registry.h
 * @brief 存储引擎注册标准化接口
 *
 * 提供统一的引擎注册机制，确保所有启用模态的存储引擎在启动时被正确注册。
 */
#ifndef DB_ENGINE_REGISTRY_H
#define DB_ENGINE_REGISTRY_H

#include "db/storage_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化所有已启用模态的存储引擎
 *
 * 根据 multimodal_config.h 中的编译开关，调用各引擎的注册函数。
 * 必须在 mm_storage_init() 之前调用。
 *
 * @return 注册成功的引擎数量，负数表示失败
 */
int engine_registry_init(void);

/**
 * @brief 注册单个存储引擎
 *
 * @param model 数据模型类型
 * @param ops 引擎操作函数表
 * @return 0 成功，-1 失败
 */
int register_storage_engine(DataModel model, const storage_ops_t *ops);

/**
 * @brief 获取已注册引擎数量
 *
 * @return 已注册引擎数量
 */
int get_registered_engine_count(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_ENGINE_REGISTRY_H */
