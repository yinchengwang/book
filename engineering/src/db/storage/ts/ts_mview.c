/**
 * @file ts_mview.c
 * @brief 时序数据物化视图实现
 */
#include "db/storage/ts/ts_mview.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

static int ensure_dir(const char *path) {
    if (path == NULL) return -1;
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;
    }
    return mkdir(path) == 0 ? 0 : -1;
}

static void get_mview_path(const ts_mview_t *mview, char *path, size_t size) {
    snprintf(path, size, "%s/mview_%s.bin", mview->data_dir, mview->name);
}

static int64_t align_timestamp(int64_t ts, int32_t granularity) {
    return (ts / granularity) * granularity;
}

/* ========================================================================
 * 生命周期 API 实现
 * ======================================================================== */

ts_mview_t *ts_mview_create(const char *name, const char *data_dir,
                            ts_agg_func_t func, int32_t granularity) {
    if (name == NULL || data_dir == NULL) return NULL;
    if (granularity <= 0) granularity = 60000;  /* 默认 1 分钟 */

    ensure_dir(data_dir);

    ts_mview_t *mview = (ts_mview_t *)calloc(1, sizeof(ts_mview_t));
    if (mview == NULL) return NULL;

    strncpy(mview->name, name, sizeof(mview->name) - 1);
    strncpy(mview->data_dir, data_dir, sizeof(mview->data_dir) - 1);
    mview->func = func;
    mview->granularity = granularity;

    mview->capacity = 1024;
    mview->records = (ts_mview_record_t *)calloc(mview->capacity,
                                                  sizeof(ts_mview_record_t));
    if (mview->records == NULL) {
        free(mview);
        return NULL;
    }

    mview->last_refresh = -1;
    mview->range_start = -1;
    mview->range_end = -1;

    LOG_INFO("物化视图创建成功: name=%s, func=%d, granularity=%dms",
             name, func, granularity);
    return mview;
}

ts_mview_t *ts_mview_open(const char *data_dir, const char *name) {
    if (data_dir == NULL || name == NULL) return NULL;

    ts_mview_t temp = {0};
    strncpy(temp.data_dir, data_dir, sizeof(temp.data_dir) - 1);
    strncpy(temp.name, name, sizeof(temp.name) - 1);

    char path[512];
    get_mview_path(&temp, path, sizeof(path));

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return NULL;

    /* 读取头 */
    uint32_t magic, version;
    ts_agg_func_t func;
    int32_t granularity;

    if (fread(&magic, sizeof(magic), 1, fp) != 1 ||
        fread(&version, sizeof(version), 1, fp) != 1 ||
        fread(&func, sizeof(func), 1, fp) != 1 ||
        fread(&granularity, sizeof(granularity), 1, fp) != 1 ||
        fread(&temp.count, sizeof(temp.count), 1, fp) != 1 ||
        fread(&temp.last_refresh, sizeof(temp.last_refresh), 1, fp) != 1 ||
        fread(&temp.range_start, sizeof(temp.range_start), 1, fp) != 1 ||
        fread(&temp.range_end, sizeof(temp.range_end), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    if (magic != TS_MVIEW_MAGIC || version != TS_MVIEW_VERSION) {
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    /* 创建物化视图 */
    ts_mview_t *mview = ts_mview_create(name, data_dir, func, granularity);
    if (mview == NULL) return NULL;

    mview->count = temp.count;
    mview->last_refresh = temp.last_refresh;
    mview->range_start = temp.range_start;
    mview->range_end = temp.range_end;

    /* 读取数据 */
    fp = fopen(path, "rb");
    if (fp != NULL) {
        fseek(fp, sizeof(uint32_t) * 3 + sizeof(ts_agg_func_t) +
              sizeof(int32_t) + sizeof(int64_t) * 3, SEEK_SET);
        fread(mview->records, sizeof(ts_mview_record_t), mview->count, fp);
        fclose(fp);
    }

    LOG_INFO("物化视图加载成功: name=%s, count=%u", name, mview->count);
    return mview;
}

int ts_mview_save(ts_mview_t *mview) {
    if (mview == NULL) return -1;

    char path[512];
    get_mview_path(mview, path, sizeof(path));

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) return -1;

    uint32_t magic = TS_MVIEW_MAGIC;
    uint32_t version = TS_MVIEW_VERSION;

    fwrite(&magic, sizeof(magic), 1, fp);
    fwrite(&version, sizeof(version), 1, fp);
    fwrite(&mview->func, sizeof(mview->func), 1, fp);
    fwrite(&mview->granularity, sizeof(mview->granularity), 1, fp);
    fwrite(&mview->count, sizeof(mview->count), 1, fp);
    fwrite(&mview->last_refresh, sizeof(mview->last_refresh), 1, fp);
    fwrite(&mview->range_start, sizeof(mview->range_start), 1, fp);
    fwrite(&mview->range_end, sizeof(mview->range_end), 1, fp);
    fwrite(mview->records, sizeof(ts_mview_record_t), mview->count, fp);

    fclose(fp);
    LOG_INFO("物化视图保存成功: name=%s, count=%u", mview->name, mview->count);
    return 0;
}

void ts_mview_destroy(ts_mview_t *mview) {
    if (mview == NULL) return;
    ts_mview_save(mview);
    free(mview->records);
    free(mview);
}

/* ========================================================================
 * 数据操作 API 实现
 * ======================================================================== */

int ts_mview_add(ts_mview_t *mview, int64_t timestamp, double value) {
    if (mview == NULL) return -1;

    int64_t aligned = align_timestamp(timestamp, mview->granularity);

    /* 查找或创建对应粒度的记录 */
    for (uint32_t i = 0; i < mview->count; i++) {
        if (mview->records[i].timestamp == aligned) {
            /* 更新现有记录 */
            mview->records[i].sum += value;
            mview->records[i].count++;
            if (value < mview->records[i].min_val || mview->records[i].count == 1) {
                mview->records[i].min_val = value;
            }
            if (value > mview->records[i].max_val) {
                mview->records[i].max_val = value;
            }
            mview->records[i].value = mview->records[i].sum / mview->records[i].count;
            return 0;
        }
    }

    /* 创建新记录 */
    if (mview->count >= mview->capacity) {
        uint32_t new_cap = mview->capacity * 2;
        ts_mview_record_t *new_rec = (ts_mview_record_t *)realloc(
            mview->records, new_cap * sizeof(ts_mview_record_t));
        if (new_rec == NULL) return -1;
        mview->records = new_rec;
        mview->capacity = new_cap;
    }

    uint32_t idx = mview->count++;
    mview->records[idx].timestamp = aligned;
    mview->records[idx].value = value;
    mview->records[idx].sum = value;
    mview->records[idx].count = 1;
    mview->records[idx].min_val = value;
    mview->records[idx].max_val = value;

    /* 更新范围 */
    if (mview->range_start < 0 || aligned < mview->range_start) {
        mview->range_start = aligned;
    }
    if (aligned > mview->range_end) {
        mview->range_end = aligned;
    }

    return 0;
}

int ts_mview_refresh(ts_mview_t *mview) {
    if (mview == NULL) return -1;

    /* 根据聚合函数计算最终值 */
    for (uint32_t i = 0; i < mview->count; i++) {
        switch (mview->func) {
            case TS_AGG_SUM:
                mview->records[i].value = mview->records[i].sum;
                break;
            case TS_AGG_AVG:
                mview->records[i].value = mview->records[i].sum / mview->records[i].count;
                break;
            case TS_AGG_MIN:
                mview->records[i].value = mview->records[i].min_val;
                break;
            case TS_AGG_MAX:
                mview->records[i].value = mview->records[i].max_val;
                break;
            case TS_AGG_COUNT:
                mview->records[i].value = (double)mview->records[i].count;
                break;
            default:
                break;
        }
    }

    mview->last_refresh = 0;  /* TODO: 获取当前时间 */
    LOG_DEBUG("物化视图刷新完成: count=%u", mview->count);
    return 0;
}

uint32_t ts_mview_query(const ts_mview_t *mview,
                        int64_t start_ts, int64_t end_ts,
                        ts_mview_record_t *results,
                        uint32_t max_results) {
    if (mview == NULL || results == NULL || max_results == 0) return 0;

    uint32_t found = 0;
    for (uint32_t i = 0; i < mview->count && found < max_results; i++) {
        if (mview->records[i].timestamp >= start_ts &&
            mview->records[i].timestamp <= end_ts) {
            results[found++] = mview->records[i];
        }
    }
    return found;
}

/* ========================================================================
 * 聚合计算 API 实现
 * ======================================================================== */

double ts_mview_aggregate(const ts_mview_record_t *records, uint32_t count,
                          ts_agg_func_t func) {
    if (records == NULL || count == 0) return 0.0;

    double result = 0.0;

    switch (func) {
        case TS_AGG_SUM:
            for (uint32_t i = 0; i < count; i++) result += records[i].sum;
            break;
        case TS_AGG_AVG: {
            double total = 0.0;
            uint64_t total_count = 0;
            for (uint32_t i = 0; i < count; i++) {
                total += records[i].sum;
                total_count += records[i].count;
            }
            result = total_count > 0 ? total / total_count : 0.0;
            break;
        }
        case TS_AGG_MIN:
            result = records[0].min_val;
            for (uint32_t i = 1; i < count; i++) {
                if (records[i].min_val < result) result = records[i].min_val;
            }
            break;
        case TS_AGG_MAX:
            result = records[0].max_val;
            for (uint32_t i = 1; i < count; i++) {
                if (records[i].max_val > result) result = records[i].max_val;
            }
            break;
        case TS_AGG_COUNT:
            for (uint32_t i = 0; i < count; i++) result += records[i].count;
            break;
        default:
            break;
    }

    return result;
}

double ts_mview_percentile(const ts_mview_record_t *records, uint32_t count,
                           double percentile) {
    if (records == NULL || count == 0 || percentile < 0 || percentile > 100) {
        return 0.0;
    }

    /* 收集所有值 */
    double *values = (double *)malloc(count * sizeof(double));
    if (values == NULL) return 0.0;

    for (uint32_t i = 0; i < count; i++) {
        values[i] = records[i].value;
    }

    /* 简单排序 */
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (values[j] < values[i]) {
                double tmp = values[i];
                values[i] = values[j];
                values[j] = tmp;
            }
        }
    }

    /* 计算百分位 */
    double idx = (percentile / 100.0) * (count - 1);
    uint32_t lower = (uint32_t)idx;
    double fraction = idx - lower;
    double result = values[lower] * (1 - fraction) + values[lower + 1] * fraction;

    free(values);
    return result;
}
