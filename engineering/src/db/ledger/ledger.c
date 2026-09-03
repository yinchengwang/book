/**
 * @file ledger.c
 * @brief 账本实现
 *
 * 实现对账模块的核心功能。
 */
#include "db/ledger/ledger.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* ========================================================================
 * 内部函数
 * ======================================================================== */

/**
 * @brief 计算账目哈希
 *
 * 使用 SHA-256 简化实现（实际应使用 crypto 库）
 */
static void compute_entry_hash(const ledger_entry_t *entry, uint8_t *hash) {
    if (entry == NULL || hash == NULL) return;

    /* 简化哈希：基于序列号、类型、金额、时间戳 */
    uint64_t data[4];
    data[0] = entry->sequence;
    data[1] = (uint64_t)entry->type;
    data[2] = (uint64_t)entry->amount;
    data[3] = entry->timestamp;

    /* 简单哈希算法 */
    uint64_t h = 0x811c9dc5;
    for (int i = 0; i < 4; i++) {
        h ^= (data[i] & 0xff);
        h *= 0x01000193;
        h ^= ((data[i] >> 8) & 0xff);
        h *= 0x01000193;
        h ^= ((data[i] >> 16) & 0xff);
        h *= 0x01000193;
        h ^= ((data[i] >> 24) & 0xff);
        h *= 0x01000193;
    }

    memset(hash, 0, 32);
    for (int i = 0; i < 8; i++) {
        hash[i] = (uint8_t)((h >> (i * 4)) & 0xff);
    }
}

/**
 * @brief 验证单条账目
 */
static bool verify_entry(const ledger_entry_t *entry) {
    if (entry == NULL) return false;

    /* 验证金额非负 */
    if (entry->amount < 0) return false;

    return true;
}

/* ========================================================================
 * 生命周期 API 实现
 * ======================================================================== */

ledger_t *ledger_create(const char *name, const char *data_dir) {
    if (name == NULL || data_dir == NULL) {
        LOG_ERROR("账本创建失败: 参数为空");
        return NULL;
    }

    ledger_t *ledger = (ledger_t *)calloc(1, sizeof(ledger_t));
    if (ledger == NULL) {
        LOG_ERROR("账本分配失败");
        return NULL;
    }

    strncpy(ledger->name, name, sizeof(ledger->name) - 1);
    strncpy(ledger->data_dir, data_dir, sizeof(ledger->data_dir) - 1);
    ledger->state = LEDGER_STATE_OK;
    ledger->current_balance = 0;
    ledger->last_sequence = 0;
    ledger->entry_count = 0;
    ledger->capacity = 1024;

    /* 分配账目数组 */
    ledger->entries = (ledger_entry_t **)calloc(ledger->capacity, sizeof(ledger_entry_t *));
    if (ledger->entries == NULL) {
        free(ledger);
        return NULL;
    }

    /* 创建数据目录 */
#ifdef _WIN32
    if (mkdir(data_dir) != 0 && errno != EEXIST) {
#else
    if (mkdir(data_dir, 0755) != 0 && errno != EEXIST) {
#endif
        LOG_WARN("账本数据目录创建失败: %s", data_dir);
    }

    LOG_INFO("账本创建成功: name=%s", name);
    return ledger;
}

ledger_t *ledger_open(const char *name, const char *data_dir) {
    if (name == NULL || data_dir == NULL) return NULL;

    /* 尝试加载现有账本 */
    ledger_t *ledger = ledger_load(name, data_dir);
    if (ledger != NULL) {
        return ledger;
    }

    /* 不存在则创建新账本 */
    return ledger_create(name, data_dir);
}

int ledger_close(ledger_t *ledger) {
    if (ledger == NULL) return -1;

    /* 保存账本 */
    int ret = ledger_save(ledger);

    LOG_INFO("账本已关闭: name=%s", ledger->name);
    return ret;
}

void ledger_destroy(ledger_t *ledger) {
    if (ledger == NULL) return;

    /* 释放所有账目 */
    for (int32_t i = 0; i < ledger->entry_count; i++) {
        free(ledger->entries[i]);
    }
    free(ledger->entries);
    free(ledger);
}

/* ========================================================================
 * 账目录入 API 实现
 * ======================================================================== */

int64_t ledger_add_entry(ledger_t *ledger,
                         ledger_entry_type_t type,
                         int64_t amount,
                         const char *idempotency_key,
                         const char *description) {
    if (ledger == NULL || amount <= 0) {
        LOG_ERROR("账目录入失败: 参数无效");
        return -1;
    }

    /* 检查幂等键 */
    if (idempotency_key != NULL && strlen(idempotency_key) > 0) {
        if (ledger_has_idempotency_key(ledger, idempotency_key)) {
            LOG_INFO("账目录入跳过: 幂等键已存在");
            /* 返回已存在的序列号 */
            for (int32_t i = 0; i < ledger->entry_count; i++) {
                if (strcmp(ledger->entries[i]->idempotency_key, idempotency_key) == 0) {
                    return (int64_t)ledger->entries[i]->sequence;
                }
            }
            return -1;
        }
    }

    /* 扩展容量 */
    if (ledger->entry_count >= ledger->capacity) {
        int32_t new_capacity = ledger->capacity * 2;
        ledger_entry_t **new_entries = (ledger_entry_t **)realloc(
            ledger->entries, (size_t)new_capacity * sizeof(ledger_entry_t *));
        if (new_entries == NULL) {
            LOG_ERROR("账目数组扩展失败");
            return -1;
        }
        ledger->entries = new_entries;
        ledger->capacity = new_capacity;
    }

    /* 创建账目 */
    ledger_entry_t *entry = (ledger_entry_t *)calloc(1, sizeof(ledger_entry_t));
    if (entry == NULL) {
        return -1;
    }

    entry->sequence = ++ledger->last_sequence;
    entry->type = type;
    entry->amount = amount;

    /* 计算余额 */
    if (type == LEDGER_ENTRY_DEBIT) {
        entry->balance_after = ledger->current_balance - amount;
    } else {
        entry->balance_after = ledger->current_balance + amount;
    }
    ledger->current_balance = entry->balance_after;

    /* 复制可选字段 */
    if (idempotency_key != NULL) {
        strncpy(entry->idempotency_key, idempotency_key,
                LEDGER_MAX_IDEMPOTENCY_KEY - 1);
    }
    if (description != NULL) {
        strncpy(entry->description, description, LEDGER_MAX_DESCRIPTION - 1);
    }
    entry->timestamp = (uint64_t)time(NULL) * 1000000;  /* 微秒 */

    /* 复制前一条哈希 */
    if (ledger->entry_count > 0) {
        memcpy(entry->prev_hash,
               ledger->entries[ledger->entry_count - 1]->entry_hash,
               32);
    }

    /* 计算本条哈希 */
    compute_entry_hash(entry, entry->entry_hash);

    /* 添加到数组 */
    ledger->entries[ledger->entry_count++] = entry;

    LOG_DEBUG("账目录入: seq=%lu, type=%d, amount=%ld, balance=%ld",
              entry->sequence, type, amount, entry->balance_after);

    return (int64_t)entry->sequence;
}

int32_t ledger_add_entries_batch(ledger_t *ledger,
                                 const ledger_entry_t *entries,
                                 int32_t count) {
    if (ledger == NULL || entries == NULL || count <= 0) return 0;

    int32_t added = 0;
    for (int32_t i = 0; i < count; i++) {
        int64_t seq = ledger_add_entry(ledger,
                                       entries[i].type,
                                       entries[i].amount,
                                       entries[i].idempotency_key[0] ?
                                           entries[i].idempotency_key : NULL,
                                       entries[i].description[0] ?
                                           entries[i].description : NULL);
        if (seq >= 0) {
            added++;
        }
    }

    return added;
}

/* ========================================================================
 * 账目查询 API 实现
 * ======================================================================== */

const ledger_entry_t *ledger_get_entry(const ledger_t *ledger, uint64_t sequence) {
    if (ledger == NULL || sequence == 0) return NULL;

    for (int32_t i = 0; i < ledger->entry_count; i++) {
        if (ledger->entries[i]->sequence == sequence) {
            return ledger->entries[i];
        }
    }

    return NULL;
}

int32_t ledger_get_last_entries(const ledger_t *ledger,
                                int32_t count,
                                ledger_entry_t *out_entries,
                                int32_t max_entries) {
    if (ledger == NULL || out_entries == NULL || max_entries <= 0) return 0;

    int32_t start = ledger->entry_count - count;
    if (start < 0) start = 0;

    int32_t num = ledger->entry_count - start;
    if (num > max_entries) num = max_entries;
    if (num > count) num = count;

    for (int32_t i = 0; i < num; i++) {
        memcpy(&out_entries[i], ledger->entries[start + i], sizeof(ledger_entry_t));
    }

    return num;
}

int32_t ledger_query_by_time(const ledger_t *ledger,
                             uint64_t start_time,
                             uint64_t end_time,
                             ledger_entry_t *out_entries,
                             int32_t max_entries) {
    if (ledger == NULL || out_entries == NULL || max_entries <= 0) return 0;

    int32_t count = 0;
    for (int32_t i = 0; i < ledger->entry_count && count < max_entries; i++) {
        if (ledger->entries[i]->timestamp >= start_time &&
            ledger->entries[i]->timestamp <= end_time) {
            memcpy(&out_entries[count++], ledger->entries[i], sizeof(ledger_entry_t));
        }
    }

    return count;
}

bool ledger_has_idempotency_key(const ledger_t *ledger, const char *idempotency_key) {
    if (ledger == NULL || idempotency_key == NULL) return false;

    for (int32_t i = 0; i < ledger->entry_count; i++) {
        if (strcmp(ledger->entries[i]->idempotency_key, idempotency_key) == 0) {
            return true;
        }
    }

    return false;
}

/* ========================================================================
 * 账本校验 API 实现
 * ======================================================================== */

bool ledger_verify_hash_chain(const ledger_t *ledger) {
    if (ledger == NULL || ledger->entry_count == 0) return true;

    for (int32_t i = 1; i < ledger->entry_count; i++) {
        /* 验证 prev_hash */
        if (memcmp(ledger->entries[i]->prev_hash,
                   ledger->entries[i - 1]->entry_hash,
                   32) != 0) {
            LOG_ERROR("账本哈希链错误: seq=%lu", ledger->entries[i]->sequence);
            return false;
        }
    }

    return true;
}

bool ledger_verify_balance(const ledger_t *ledger) {
    if (ledger == NULL) return false;

    int64_t calculated = 0;
    for (int32_t i = 0; i < ledger->entry_count; i++) {
        if (ledger->entries[i]->type == LEDGER_ENTRY_DEBIT) {
            calculated -= ledger->entries[i]->amount;
        } else {
            calculated += ledger->entries[i]->amount;
        }
    }

    return calculated == ledger->current_balance;
}

ledger_reconcile_result_t *ledger_reconcile(ledger_t *ledger) {
    if (ledger == NULL) return NULL;

    ledger_reconcile_result_t *result =
        (ledger_reconcile_result_t *)calloc(1, sizeof(ledger_reconcile_result_t));
    if (result == NULL) return NULL;

    result->entry_count = ledger->entry_count;
    result->actual_balance = ledger->current_balance;

    /* 计算借方和贷方总额 */
    for (int32_t i = 0; i < ledger->entry_count; i++) {
        if (ledger->entries[i]->type == LEDGER_ENTRY_DEBIT) {
            result->total_debit += ledger->entries[i]->amount;
        } else {
            result->total_credit += ledger->entries[i]->amount;
        }
    }

    result->expected_balance = result->total_credit - result->total_debit;
    result->discrepancy = result->expected_balance - result->actual_balance;
    result->is_balanced = (result->discrepancy == 0);

    /* 验证哈希链 */
    for (int32_t i = 1; i < ledger->entry_count; i++) {
        if (memcmp(ledger->entries[i]->prev_hash,
                   ledger->entries[i - 1]->entry_hash,
                   32) != 0) {
            result->hash_chain_errors++;
        }
    }

    /* 检查重复幂等键 */
    for (int32_t i = 0; i < ledger->entry_count; i++) {
        if (ledger->entries[i]->idempotency_key[0] == '\0') continue;
        for (int32_t j = i + 1; j < ledger->entry_count; j++) {
            if (strcmp(ledger->entries[i]->idempotency_key,
                       ledger->entries[j]->idempotency_key) == 0) {
                result->duplicate_keys++;
                break;
            }
        }
    }

    LOG_INFO("账本对账完成: balanced=%d, discrepancy=%ld, hash_errors=%d",
             result->is_balanced, result->discrepancy, result->hash_chain_errors);

    return result;
}

void ledger_free_reconcile_result(ledger_reconcile_result_t *result) {
    free(result);
}

/* ========================================================================
 * 持久化 API 实现
 * ======================================================================== */

static int get_ledger_file_path(const ledger_t *ledger, char *path, size_t size) {
    snprintf(path, size, "%s/%s.ledger", ledger->data_dir, ledger->name);
    return 0;
}

int ledger_save(const ledger_t *ledger) {
    if (ledger == NULL) return -1;

    char path[512];
    get_ledger_file_path(ledger, path, sizeof(path));

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        LOG_ERROR("账本文件创建失败: %s", path);
        return -1;
    }

    /* 写入魔数 */
    uint32_t magic = LEDGER_FILE_MAGIC;
    fwrite(&magic, sizeof(magic), 1, fp);

    /* 写入版本 */
    uint32_t version = LEDGER_FILE_VERSION;
    fwrite(&version, sizeof(version), 1, fp);

    /* 写入状态 */
    fwrite(&ledger->current_balance, sizeof(ledger->current_balance), 1, fp);
    fwrite(&ledger->last_sequence, sizeof(ledger->last_sequence), 1, fp);
    fwrite(&ledger->entry_count, sizeof(ledger->entry_count), 1, fp);

    /* 写入账目 */
    for (int32_t i = 0; i < ledger->entry_count; i++) {
        fwrite(ledger->entries[i], sizeof(ledger_entry_t), 1, fp);
    }

    fclose(fp);
    LOG_INFO("账本已保存: name=%s, entries=%d", ledger->name, ledger->entry_count);
    return 0;
}

ledger_t *ledger_load(const char *name, const char *data_dir) {
    if (name == NULL || data_dir == NULL) return NULL;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ledger", data_dir, name);

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    /* 验证魔数 */
    uint32_t magic;
    if (fread(&magic, sizeof(magic), 1, fp) != 1 || magic != LEDGER_FILE_MAGIC) {
        LOG_WARN("账本文件魔数无效");
        fclose(fp);
        return NULL;
    }

    /* 验证版本 */
    uint32_t version;
    if (fread(&version, sizeof(version), 1, fp) != 1 || version != LEDGER_FILE_VERSION) {
        LOG_WARN("账本文件版本不匹配");
        fclose(fp);
        return NULL;
    }

    /* 创建账本 */
    ledger_t *ledger = ledger_create(name, data_dir);
    if (ledger == NULL) {
        fclose(fp);
        return NULL;
    }

    /* 读取状态 */
    fread(&ledger->current_balance, sizeof(ledger->current_balance), 1, fp);
    fread(&ledger->last_sequence, sizeof(ledger->last_sequence), 1, fp);
    fread(&ledger->entry_count, sizeof(ledger->entry_count), 1, fp);

    /* 读取账目 */
    for (int32_t i = 0; i < ledger->entry_count; i++) {
        ledger_entry_t *entry = (ledger_entry_t *)malloc(sizeof(ledger_entry_t));
        if (entry != NULL && fread(entry, sizeof(ledger_entry_t), 1, fp) == 1) {
            if (i >= ledger->capacity) {
                ledger->capacity = i * 2;
                ledger->entries = (ledger_entry_t **)realloc(
                    ledger->entries, (size_t)ledger->capacity * sizeof(ledger_entry_t *));
            }
            ledger->entries[i] = entry;
        } else {
            free(entry);
            break;
        }
    }

    fclose(fp);
    LOG_INFO("账本已加载: name=%s, entries=%d", name, ledger->entry_count);
    return ledger;
}

/* ========================================================================
 * 统计 API 实现
 * ======================================================================== */

void ledger_get_stats(const ledger_t *ledger,
                      int64_t *out_balance,
                      uint64_t *out_count,
                      int64_t *out_debit,
                      int64_t *out_credit) {
    if (ledger == NULL) return;

    if (out_balance) *out_balance = ledger->current_balance;
    if (out_count) *out_count = ledger->entry_count;

    int64_t debit = 0, credit = 0;
    for (int32_t i = 0; i < ledger->entry_count; i++) {
        if (ledger->entries[i]->type == LEDGER_ENTRY_DEBIT) {
            debit += ledger->entries[i]->amount;
        } else {
            credit += ledger->entries[i]->amount;
        }
    }

    if (out_debit) *out_debit = debit;
    if (out_credit) *out_credit = credit;
}
