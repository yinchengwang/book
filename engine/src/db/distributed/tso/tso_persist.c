/* tso_persist.c —— TSO 水位线持久化（故障重启时间戳不倒退）
   纯文件 IO：保存/读回"已分配峰值"，读写结构体字段通过公开接口
   tso_oracle_peak（实现于 tso_codec.c，字段可见处）间接完成，保持封装。 */
#include "distributed/tso.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/* 保存当前已分配最大值（水位）到文件。简洁起见每次覆盖写单条 int64。 */
int tso_persist_save(const tso_oracle_t *o, const char *path) {
    if (!o || !path) return -1;
    int64_t peak = tso_oracle_peak(o);
    FILE *f = fopen(path, "wb");
    if (!f) return -errno;
    size_t n = fwrite(&peak, sizeof(peak), 1, f);
    fclose(f);
    return n == 1 ? 0 : -errno;
}

/* 读回持久化水位；文件缺失或损坏时返回 0（视为无水位）。 */
int64_t tso_persist_load(const char *path) {
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    int64_t peak = 0;
    if (fread(&peak, sizeof(peak), 1, f) != 1) peak = 0;
    fclose(f);
    return peak;
}