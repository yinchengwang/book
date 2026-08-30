/**
 * @file netconf_server.h
 * @brief NETCONF 协议服务器接口
 *
 * 实现 NETCONF 1.0（RFC 6241）的核心 RPC 操作：
 *   - get              : 读取运行数据
 *   - get-config       : 读取配置数据
 *   - edit-config      : 编辑配置（merge/replace/create/delete 操作）
 *   - copy-config      : 复制源 datastore 内容到目标
 *   - delete-config    : 删除目标 datastore
 *   - lock / unlock    : datastore 锁（占位）
 *
 * 协议层使用手写的简易 XML 解析，rpc 请求与 reply 都是 XML 文本。
 */
#ifndef DB_NETCONF_SERVER_H
#define DB_NETCONF_SERVER_H

#include "db/yang/yang_data.h"
#include "db/yang/yang_model.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 默认 datastore 名称 */
#define NETCONF_DS_RUNNING    "running"
#define NETCONF_DS_STARTUP    "startup"
#define NETCONF_DS_CANDIDATE  "candidate"

/** NETCONF XML 命名空间（占位） */
#define NETCONF_NS            "urn:ietf:params:xml:ns:netconf:base:1.0"
#define NETCONF_NS_1_1        "urn:ietf:params:xml:ns:netconf:base:1.1"

/** 默认消息缓冲区大小 */
#define NETCONF_BUF_SIZE      (16 * 1024)

/* ============================================================
 * 错误码
 * ============================================================ */

typedef enum netconf_result_e {
    NETCONF_OK = 0,
    NETCONF_ERR_INVALID_RPC = -1,        /**< RPC 格式错误 */
    NETCONF_ERR_UNKNOWN_OP = -2,         /**< 未知操作 */
    NETCONF_ERR_DATABASE = -3,           /**< datastore 错误 */
    NETCONF_ERR_NOT_FOUND = -4,          /**< 数据未找到 */
    NETCONF_ERR_INTERNAL = -5,           /**< 内部错误 */
    NETCONF_ERR_INVALID_VALUE = -6,      /**< 非法值 */
} netconf_result_t;

/* ============================================================
 * edit-config 操作类型
 * ============================================================ */

typedef enum netconf_edit_op_e {
    NETCONF_EDIT_MERGE = 0,    /**< 合并（默认） */
    NETCONF_EDIT_REPLACE,      /**< 替换 */
    NETCONF_EDIT_CREATE,       /**< 创建 */
    NETCONF_EDIT_DELETE,       /**< 删除 */
    NETCONF_EDIT_REMOVE,       /**< 移除（不要求存在） */
} netconf_edit_op_t;

/* ============================================================
 * NETCONF 会话
 * ============================================================ */

/**
 * @brief NETCONF 会话（内存态，无网络）
 */
typedef struct netconf_session_s {
    char session_id[64];           /**< 会话标识 */
    char *running_ds_name;        /**< 运行 datastore 名 */
    yang_datastore_t *running;    /**< 运行 datastore */
    yang_datastore_t *startup;    /**< 启动 datastore（可选） */
    yang_datastore_t *candidate;  /**< 候选 datastore（可选） */
    yang_model_t *model;          /**< YANG 模型（可选，用于校验） */
    uint32_t message_id;          /**< 当前消息 ID */
    bool locked;                  /**< 是否锁住 */
} netconf_session_t;

/* ============================================================
 * 会话生命周期
 * ============================================================ */

/**
 * @brief 创建 NETCONF 会话
 * @param session_id 会话 ID 字符串
 * @return 会话指针，失败返回 NULL
 *
 * 创建会话默认初始化 running / startup / candidate 三个 datastore。
 */
netconf_session_t *netconf_session_create(const char *session_id);

/**
 * @brief 释放会话
 * @param s 会话
 */
void netconf_session_free(netconf_session_t *s);

/**
 * @brief 设置 YANG 模型（可选）
 * @param s 会话
 * @param model 模型指针（可为 NULL）
 */
void netconf_session_set_model(netconf_session_t *s, yang_model_t *model);

/**
 * @brief 获取指定 datastore
 * @param s 会话
 * @param name datastore 名（"running"/"startup"/"candidate"）
 * @return datastore 指针，未找到返回 NULL
 */
yang_datastore_t *netconf_session_get_datastore(netconf_session_t *s,
                                                const char *name);

/* ============================================================
 * RPC 处理
 * ============================================================ */

/**
 * @brief 处理 RPC 请求并生成 reply XML
 * @param s 会话
 * @param rpc_in RPC 请求 XML
 * @param rpc_len 输入长度（0 表示 strlen）
 * @param reply_out 输出 reply 缓冲区
 * @param reply_size 缓冲区大小
 * @return NETCONF_OK 或错误码
 *
 * reply_out 内容为 NETCONF reply XML。
 * 支持 NETCONF 1.0 和 1.1 RPC：get、get-config、edit-config、copy-config、
 * delete-config、get-capabilities。
 */
netconf_result_t netconf_handle_rpc(netconf_session_t *s,
                                    const char *rpc_in, size_t rpc_len,
                                    char *reply_out, size_t reply_size);

/**
 * @brief get 操作：返回指定过滤器的子树
 * @param s 会话
 * @param filter 子树根节点（NULL 表示全树）
 * @param reply_out 输出缓冲区
 * @param reply_size 缓冲区大小
 * @return NETCONF_OK 或错误码
 */
netconf_result_t netconf_op_get(netconf_session_t *s,
                                const yang_data_node_t *filter,
                                char *reply_out, size_t reply_size);

/**
 * @brief get-config 操作：读取指定 datastore 的子树
 * @param s 会话
 * @param source 源 datastore 名
 * @param filter 子树根节点（NULL 表示全树）
 * @param reply_out 输出缓冲区
 * @param reply_size 缓冲区大小
 * @return NETCONF_OK 或错误码
 */
netconf_result_t netconf_op_get_config(netconf_session_t *s,
                                       const char *source,
                                       const yang_data_node_t *filter,
                                       char *reply_out, size_t reply_size);

/**
 * @brief edit-config 操作：编辑 datastore
 * @param s 会话
 * @param target 目标 datastore 名
 * @param config 编辑后的配置子树
 * @param op 编辑操作
 * @return NETCONF_OK 或错误码
 */
netconf_result_t netconf_op_edit_config(netconf_session_t *s,
                                        const char *target,
                                        const yang_data_node_t *config,
                                        netconf_edit_op_t op);

/**
 * @brief copy-config 操作
 * @param s 会话
 * @param source 源（datastore 名或 "<config>...</config>" 字面量）
 * @param target 目标 datastore 名
 * @return NETCONF_OK 或错误码
 *
 * 简化实现：source 为 datastore 名时，将整个 datastore 内容拷贝到 target。
 */
netconf_result_t netconf_op_copy_config(netconf_session_t *s,
                                        const char *source,
                                        const char *target);

/**
 * @brief delete-config 操作
 * @param s 会话
 * @param target 目标 datastore 名
 * @return NETCONF_OK 或错误码（不能删除 running）
 */
netconf_result_t netconf_op_delete_config(netconf_session_t *s,
                                          const char *target);

/* ============================================================
 * XML 工具
 * ============================================================ */

/**
 * @brief 将数据子树序列化为 NETCONF XML
 * @param root 数据根节点
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入字节数（不含终止符），-1 失败
 */
int netconf_xml_serialize(const yang_data_node_t *root,
                          char *buf, size_t buf_size);

/**
 * @brief 简易 XML 解析：解析 RPC 中的子元素
 *
 * 在 buf 中查找 "<name>...</name>" 或 "<name/>"，将内容提取到 out 中。
 * 返回指向匹配结束之后的下一个位置（找不到返回 NULL）。
 */
const char *netconf_xml_find(const char *buf, const char *name,
                             char *out, size_t out_size);

/**
 * @brief 简易 XML 解析：从 XML 提取指定子树的 name 与 value
 *
 * 返回节点数；out_names 与 out_values 平行数组；用于 edit-config 解析。
 */
int netconf_xml_parse_subtree(const char *buf,
                              char (*out_names)[YANG_MAX_NAME_LEN],
                              char (*out_values)[YANG_MAX_VALUE_LEN],
                              int max_count);

#ifdef __cplusplus
}
#endif

#endif /* DB_NETCONF_SERVER_H */