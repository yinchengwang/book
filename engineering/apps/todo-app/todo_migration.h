#ifndef TODO_MIGRATION_H
#define TODO_MIGRATION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 从旧版数据迁移
 * @return 0 无需迁移或迁移成功，1 迁移发生，-1 迁移失败
 */
int todo_migrate_from_legacy(void);

/**
 * @brief 检查是否存在旧数据文件
 * @return true 存在旧文件
 */
bool todo_has_legacy_data(void);

#ifdef __cplusplus
}
#endif

#endif /* TODO_MIGRATION_H */
