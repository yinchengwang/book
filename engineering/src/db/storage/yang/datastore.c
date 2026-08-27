#include "db/datastore.h"
#include "db/core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

datastore_t *datastore_load(const char *path, datastore_type_t type) {
    if (!path) return NULL;
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        LOG_INFO("datastore_load: %s 不存在，按空库启动", path);
        datastore_t *ds = calloc(1, sizeof(datastore_t));
        if (!ds) return NULL;
        ds->type = type;
        ds->path = strdup(path);
        return ds;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz < 0) { fclose(fp); return NULL; }
    datastore_t *ds = calloc(1, sizeof(datastore_t));
    if (!ds) { fclose(fp); return NULL; }
    ds->type = type;
    ds->path = strdup(path);
    ds->xml_data = malloc(sz + 1);
    if (!ds->xml_data) { fclose(fp); datastore_free(ds); return NULL; }
    fread(ds->xml_data, 1, sz, fp);
    ds->xml_data[sz] = '\0';
    ds->xml_len = (size_t)sz;
    fclose(fp);
    return ds;
}

int datastore_save(const datastore_t *ds) {
    if (!ds || !ds->path) return -1;
    /* 写临时文件 + rename 原子替换 */
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp", ds->path);
    FILE *fp = fopen(tmp, "wb");
    if (!fp) return -1;
    if (ds->xml_data && ds->xml_len > 0) {
        fwrite(ds->xml_data, 1, ds->xml_len, fp);
    }
    fflush(fp);
    /* fsync (C0-2 统一策略) */
#ifdef _WIN32
    _commit(_fileno(fp));
#else
    fsync(fileno(fp));
#endif
    fclose(fp);
    /* 原子 rename */
    if (rename(tmp, ds->path) != 0) {
        LOG_ERROR("datastore_save: rename %s → %s 失败", tmp, ds->path);
        return -1;
    }
    return 0;
}

int datastore_commit(datastore_t *running, datastore_t *candidate) {
    if (!running || !candidate) return -1;
    if (candidate->type != DATASTORE_CANDIDATE) return -1;
    /* 校验：candidate XML 解析有效 */
    /* 原子：写 candidate 到 running 文件 */
    free(running->xml_data);
    running->xml_data = candidate->xml_data ? strdup(candidate->xml_data) : NULL;
    running->xml_len = candidate->xml_len;
    return datastore_save(running);
}

void datastore_free(datastore_t *ds) {
    if (!ds) return;
    free(ds->xml_data);
    free(ds->path);
    free(ds);
}