#ifndef TODO_CHANGE_H
#define TODO_CHANGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建 OPSX 变更
 * @param todo_id 待办 ID
 * @param out_change_id 输出变更 ID
 * @param change_id_size out_change_id 缓冲区大小
 * @param out_url 输出变更 URL
 * @param url_size out_url 缓冲区大小
 * @return 0 成功，-1 失败
 */
int todo_create_change(int64_t todo_id, char *out_change_id, size_t change_id_size,
                      char *out_url, size_t url_size);

#ifdef __cplusplus
}
#endif

#endif /* TODO_CHANGE_H */
