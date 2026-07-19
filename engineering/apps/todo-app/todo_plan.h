#ifndef TODO_PLAN_H
#define TODO_PLAN_H

#include "todo_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 预置模板标识 */
#define TEMPLATE_DB_LEARNING 1

/* 导入预置模板 */
int plan_import_template(int template_id, int64_t start_date, int64_t task_system_id);

/* 展开计划为 Todo */
int plan_expand_to_todos(int64_t plan_id, int64_t task_system_id);

/* 获取计划中的阶段进度 */
int plan_get_phase_progress(int64_t plan_id, double *progress, int *count);

#ifdef __cplusplus
}
#endif

#endif /* TODO_PLAN_H */
