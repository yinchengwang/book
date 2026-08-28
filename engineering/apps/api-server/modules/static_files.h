#ifndef STATIC_FILES_MODULE_H
#define STATIC_FILES_MODULE_H

#include "../../common/http_router.h"

/**
 * @brief 注册静态文件服务（设置静态目录）
 */
void register_static_files(Router *r, const char *static_dir);

/**
 * @brief 提供静态文件服务（由 request_callback 调用）
 *
 * @param req       HTTP 请求
 * @param resp      HTTP 响应（输出）
 * @param static_dir 静态文件根目录
 * @return 0 成功处理，-1 未找到
 */
int static_files_serve(const HttpRequest *req, HttpResponse *resp, const char *static_dir);

#endif /* STATIC_FILES_MODULE_H */