/* ============================================================
 * todo_view.h - 视图系统
 *
 * 飞书多维表格风格视图：表格/看板/日历/甘特图
 * ============================================================ */

#ifndef TODO_VIEW_H
#define TODO_VIEW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 视图类型 */
typedef enum {
    VIEW_TYPE_TABLE = 0,    /* 表格视图 */
    VIEW_TYPE_BOARD = 1,    /* 看板视图 */
    VIEW_TYPE_CALENDAR = 2, /* 日历视图 */
    VIEW_TYPE_GANTT = 3,    /* 甘特图 */
    VIEW_TYPE_COUNT
} view_type_t;

/* 视图定义 */
typedef struct {
    int64_t id;
    char name[64];          /* 视图名称 */
    view_type_t type;       /* 视图类型 */
    char config[4096];      /* 视图配置 JSON */
    int is_default;         /* 是否默认视图 */
    int sort_order;         /* 排序顺序 */
    int64_t created_at;
} view_t;

/* 视图 CRUD */
int view_create(const char *name, view_type_t type, const char *config, int is_default, int64_t *out_id);
int view_get(int64_t id, view_t *out);
/* name/config/is_default 为 NULL 时不更新对应字段 */
int view_update(int64_t id, const char *name, const char *config, int *is_default);
int view_delete(int64_t id);
int view_list(view_t **out, int *out_count);
int view_set_default(int64_t id);
int view_get_default(view_t *out);
int view_update_sort(int64_t id, int sort_order);

/* 视图类型转换 */
const char *view_type_to_string(view_type_t type);
view_type_t view_type_from_string(const char *str);

/* 内存清理 */
void view_free_list(view_t *list, int count);

#ifdef __cplusplus
}
#endif

#endif /* TODO_VIEW_H */
