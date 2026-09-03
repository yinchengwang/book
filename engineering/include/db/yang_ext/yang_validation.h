/**
 * @file yang_validation.h
 * @brief YANG 数据模型验证接口
 *
 * Phase12 - 实现 YANG 验证，追赶 NETCONF 水平。
 */
#ifndef DB_YANG_VALIDATION_H
#define DB_YANG_VALIDATION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** YANG 模型不透明类型 */
typedef struct yang_model yang_model_t;

/** 验证结果 */
typedef struct {
    bool valid;
    char error[512];
    int error_line;
} yang_validation_result_t;

/** 加载 YANG 模型 */
yang_model_t *yang_model_load(const char *yang_content, size_t len);

/** 验证 XML 数据 */
yang_validation_result_t yang_validate(yang_model_t *model, const char *xml_data, size_t len);

/** 验证 JSON 数据 */
yang_validation_result_t yang_validate_json(yang_model_t *model, const char *json_data, size_t len);

/** 销毁模型 */
void yang_model_destroy(yang_model_t *model);

#ifdef __cplusplus
}
#endif
#endif /* DB_YANG_VALIDATION_H */
