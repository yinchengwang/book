/**
 * @file copy.c
 * @brief 数据导入导出工具核心实现
 */
#include "db/tools/copy.h"
#include <stdlib.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────
 * 上下文结构
 * ───────────────────────────────────────────────────────────────── */

struct CopyContext {
    CopyOptions options;
    int row_count;
    int error_count;
    char error_msg[256];
};

/* ─────────────────────────────────────────────────────────────────
 * 公共 API 实现
 * ───────────────────────────────────────────────────────────────── */

CopyOptions copy_default_options(void)
{
    CopyOptions opts = {
        .format = COPY_FORMAT_CSV,
        .delimiter = ',',
        .quote_char = '"',
        .escape_char = '"',
        .null_string = "\\N",
        .header = true
    };
    return opts;
}

CopyContext *copy_create(const CopyOptions *opts)
{
    CopyContext *ctx = (CopyContext *)malloc(sizeof(CopyContext));
    if (!ctx) {
        return NULL;
    }

    if (opts) {
        ctx->options = *opts;
    } else {
        ctx->options = copy_default_options();
    }

    ctx->row_count = 0;
    ctx->error_count = 0;
    memset(ctx->error_msg, 0, sizeof(ctx->error_msg));

    return ctx;
}

void copy_destroy(CopyContext *ctx)
{
    if (ctx) {
        free(ctx);
    }
}

const char *copy_get_error(const CopyContext *ctx)
{
    if (!ctx) {
        return "NULL context";
    }
    return ctx->error_msg[0] ? ctx->error_msg : "no error";
}

int copy_get_row_count(const CopyContext *ctx)
{
    return ctx ? ctx->row_count : 0;
}

int copy_get_error_count(const CopyContext *ctx)
{
    return ctx ? ctx->error_count : 0;
}