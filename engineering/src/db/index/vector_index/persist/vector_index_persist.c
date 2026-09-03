/**
 * @file vector_index_persist.c
 * @brief 向量索引统一持久化格式实现
 *
 * 提供统一的文件头读写、校验和计算、版本兼容性检查等功能。
 */
#define _POSIX_C_SOURCE 200809L

#include <db/index/vector_index/persist/vector_index_persist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

/* ========================================================================
 * CRC32 校验和实现
 * ======================================================================== */

/** CRC32 查找表 */
static uint32_t crc32_table[256] = {0};
static int crc32_table_init = 0;

/**
 * @brief 初始化 CRC32 查找表
 */
static void init_crc32_table(void) {
    if (crc32_table_init) return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ VECTOR_INDEX_CRC32_POLY;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_init = 1;
}

/**
 * @brief 计算 CRC32 校验和
 */
uint32_t vector_index_crc32(const void *data, size_t size) {
    if (!data || size == 0) return 0;

    init_crc32_table();

    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < size; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ p[i]) & 0xFF];
    }
    return ~crc;
}

/* ========================================================================
 * 校验和验证
 * ======================================================================== */

/**
 * @brief 验证头部校验和
 */
bool vector_index_verify_header_checksum(const vector_index_header_t *header) {
    if (!header) return false;

    uint32_t stored = header->header_checksum;
    vector_index_header_t temp = *header;
    temp.header_checksum = 0;

    uint32_t computed = vector_index_crc32(&temp, VECTOR_INDEX_CHECKSUM_OFFSET);
    return stored == computed;
}

/**
 * @brief 计算并设置头部校验和
 */
void vector_index_set_header_checksum(vector_index_header_t *header) {
    if (!header) return;

    header->header_checksum = 0;
    header->header_checksum = vector_index_crc32(header, VECTOR_INDEX_CHECKSUM_OFFSET);
}

/**
 * @brief 验证文件头
 */
bool vector_index_verify_header(const vector_index_header_t *header) {
    if (!header) return false;

    /* 检查魔数 */
    if (header->magic != VECTOR_INDEX_MAGIC) {
        return false;
    }

    /* 检查版本 */
    if (header->version == 0 || header->version > VECTOR_INDEX_MAX_SUPPORTED_VERSION) {
        return false;
    }

    /* 检查校验和 */
    if (!vector_index_verify_header_checksum(header)) {
        return false;
    }

    /* 检查维度 */
    if (header->dims <= 0) {
        return false;
    }

    return true;
}

/* ========================================================================
 * 文件操作
 * ======================================================================== */

/**
 * @brief 读取文件头
 */
int vector_index_read_header(FILE *fp, vector_index_header_t *header) {
    if (!fp || !header) return -1;

    memset(header, 0, sizeof(*header));

    if (fread(header, sizeof(*header), 1, fp) != 1) {
        return -1;
    }

    return vector_index_verify_header(header) ? 0 : -2;
}

/**
 * @brief 写入文件头
 */
int vector_index_write_header(FILE *fp, const vector_index_header_t *header) {
    if (!fp || !header) return -1;

    vector_index_header_t temp = *header;
    vector_index_set_header_checksum(&temp);

    if (fwrite(&temp, sizeof(temp), 1, fp) != 1) {
        return -1;
    }

    return 0;
}

/**
 * @brief 读取索引数据区
 */
size_t vector_index_read_data(FILE *fp, uint64_t offset, size_t size, void *buffer) {
    if (!fp || !buffer || size == 0) return 0;

    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        return 0;
    }

    size_t read = fread(buffer, 1, size, fp);
    return read;
}

/**
 * @brief 写入索引数据区
 */
size_t vector_index_write_data(FILE *fp, uint64_t offset, size_t size, const void *data) {
    if (!fp || !data || size == 0) return 0;

    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        return 0;
    }

    size_t written = fwrite(data, 1, size, fp);
    return written;
}

/* ========================================================================
 * 版本兼容性
 * ======================================================================== */

/**
 * @brief 检查版本兼容性
 */
bool vector_index_check_version_compatibility(uint32_t version) {
    return version > 0 && version <= VECTOR_INDEX_MAX_SUPPORTED_VERSION;
}

/**
 * @brief 获取升级建议
 */
char *vector_index_get_upgrade_hint(uint32_t from_version, uint32_t to_version) {
    (void)to_version;

    char *hint = (char *)malloc(256);
    if (!hint) return NULL;

    if (from_version > VECTOR_INDEX_MAX_SUPPORTED_VERSION) {
        snprintf(hint, 256,
                 "版本 %u 超出最大支持版本 %u。"
                 "建议升级程序到最新版本以支持此索引文件。",
                 from_version, VECTOR_INDEX_MAX_SUPPORTED_VERSION);
    } else if (from_version == 0) {
        snprintf(hint, 256, "版本号无效，无法读取此索引文件。");
    } else {
        snprintf(hint, 256, "版本兼容，无需升级。");
    }

    return hint;
}

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 检测文件类型
 */
vector_index_type_t vector_index_detect_type(const char *path) {
    if (!path) return VECTOR_INDEX_TYPE_UNKNOWN;

    FILE *fp = fopen(path, "rb");
    if (!fp) return VECTOR_INDEX_TYPE_UNKNOWN;

    uint32_t magic = 0;
    fread(&magic, sizeof(magic), 1, fp);
    fclose(fp);

    if (magic == VECTOR_INDEX_MAGIC) {
        /* 读取头部获取索引类型 */
        FILE *fp2 = fopen(path, "rb");
        if (fp2) {
            vector_index_header_t header;
            if (fread(&header, sizeof(header), 1, fp2) == 1) {
                fclose(fp2);
                return (vector_index_type_t)header.index_type;
            }
            fclose(fp2);
        }
    }

    /* 尝试检测其他格式 */
    /* HNSW 格式 */
    if (magic == 0x484E5357) {  /* "HNSW" */
        return VECTOR_INDEX_TYPE_HNSW;
    }

    /* IVF-PQ 格式 */
    uint64_t ivf_magic_check;
    FILE *fp3 = fopen(path, "rb");
    if (fp3) {
        fseek(fp3, 0, SEEK_SET);
        if (fread(&ivf_magic_check, sizeof(ivf_magic_check), 1, fp3) == 1) {
            /* 检查 IVF_PQ_MAGIC 的高 32 位 */
            if ((ivf_magic_check >> 32) == 0x49565085) {  /* "IVPQ" */
                fclose(fp3);
                return VECTOR_INDEX_TYPE_IVF_PQ;
            }
        }
        fclose(fp3);
    }

    return VECTOR_INDEX_TYPE_UNKNOWN;
}

/**
 * @brief 获取文件大小
 */
int64_t vector_index_file_size(const char *path) {
    if (!path) return -1;

    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    return (int64_t)st.st_size;
}

/* vector_index_is_valid() 定义在 vector_engine.c 中 */

int vector_index_get_summary(const char *path, char *summary) {
    if (!path || !summary) return -1;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        snprintf(summary, 256, "无法打开文件: %s", path);
        return -1;
    }

    vector_index_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        snprintf(summary, 256, "读取头部失败");
        return -1;
    }
    fclose(fp);

    /* 生成摘要 */
    const char *type_name = vector_index_type_name((vector_index_type_t)header.index_type);
    time_t created = (time_t)header.created_at;
    time_t modified = (time_t)header.modified_at;

    char created_str[64] = "未知";
    char modified_str[64] = "未知";

#ifdef _WIN32
    struct tm tm_created, tm_modified;
    if (localtime_s(&tm_created, &created) == 0) {
        strftime(created_str, sizeof(created_str), "%Y-%m-%d %H:%M:%S", &tm_created);
    }
    if (localtime_s(&tm_modified, &modified) == 0) {
        strftime(modified_str, sizeof(modified_str), "%Y-%m-%d %H:%M:%S", &tm_modified);
    }
#else
    struct tm *tm_created = localtime(&created);
    struct tm *tm_modified = localtime(&modified);
    if (tm_created) strftime(created_str, sizeof(created_str), "%Y-%m-%d %H:%M:%S", tm_created);
    if (tm_modified) strftime(modified_str, sizeof(modified_str), "%Y-%m-%d %H:%M:%S", tm_modified);
#endif

    snprintf(summary, 256,
             "类型: %s | 版本: %u | 维度: %d | 向量数: %lu | "
             "大小: %lu bytes | 创建: %s | 修改: %s",
             type_name, header.version, header.dims,
             (unsigned long)header.n_total,
             (unsigned long)header.data_size,
             created_str, modified_str);

    return 0;
}
