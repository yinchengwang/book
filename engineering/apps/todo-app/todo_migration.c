#include "todo_migration.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define LEGACY_FILE  "issue_tool.json"
#define BACKUP_FILE  "issue_tool.json.bak"
#define NEW_FILE     "todo_app.json"

/* ============================================================
 * 工具函数
 * ============================================================ */
static int64_t now_ts(void) {
    return (int64_t)time(NULL);
}

/* ============================================================
 * 检查旧文件是否存在
 * ============================================================ */
bool todo_has_legacy_data(void) {
    FILE *fp = fopen(LEGACY_FILE, "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

/* ============================================================
 * 迁移核心逻辑
 * ============================================================ */
static int migrate_data(void) {
    FILE *fp = fopen(LEGACY_FILE, "r");
    if (!fp) return 0;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); return 0; }

    char *buf = malloc(sz + 1);
    if (!buf) { fclose(fp); return -1; }
    fread(buf, 1, sz, fp);
    buf[sz] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        fprintf(stderr, "[迁移] 旧数据 JSON 解析失败\n");
        return -1;
    }

    /* 构建新格式数据 */
    cJSON *new_root = cJSON_CreateObject();

    /* next_id */
    cJSON *jnext = cJSON_GetObjectItem(root, "next_issue_id");
    int64_t next_id = jnext ? (int64_t)jnext->valuedouble : 1;
    cJSON_AddNumberToObject(new_root, "next_todo_id", next_id);

    cJSON *jnext_check = cJSON_GetObjectItem(root, "next_check_id");
    cJSON_AddNumberToObject(new_root, "next_check_id", jnext_check ? (int64_t)jnext_check->valuedouble : 1);

    cJSON_AddNumberToObject(new_root, "next_group_id", 1);
    cJSON_AddNumberToObject(new_root, "next_comment_id", 1);

    /* 迁移 todos */
    cJSON *jissues = cJSON_GetObjectItem(root, "issues");
    cJSON *jtodos = cJSON_CreateArray();

    if (jissues && cJSON_IsArray(jissues)) {
        int n = cJSON_GetArraySize(jissues);
        for (int i = 0; i < n; i++) {
            cJSON *jissue = cJSON_GetArrayItem(jissues, i);
            cJSON *jtodo = cJSON_CreateObject();

            cJSON *jid = cJSON_GetObjectItem(jissue, "id");
            cJSON_AddItemReferenceToObject(jtodo, "id", jid ? jid : cJSON_CreateNumber(0));

            cJSON *jtitle = cJSON_GetObjectItem(jissue, "title");
            cJSON_AddItemReferenceToObject(jtodo, "title", jtitle ? jtitle : cJSON_CreateString(""));

            cJSON *jdesc = cJSON_GetObjectItem(jissue, "description");
            cJSON_AddItemReferenceToObject(jtodo, "description", jdesc ? jdesc : cJSON_CreateString(""));

            cJSON *jstatus = cJSON_GetObjectItem(jissue, "status");
            cJSON_AddItemReferenceToObject(jtodo, "status", jstatus ? jstatus : cJSON_CreateString("open"));

            /* 标签 */
            cJSON *jlabels = cJSON_GetObjectItem(jissue, "labels");
            if (jlabels) {
                cJSON_AddItemReferenceToObject(jtodo, "labels", jlabels);
            } else {
                cJSON_AddItemToObject(jtodo, "labels", cJSON_CreateArray());
            }

            /* 新增字段默认值 */
            cJSON_AddNumberToObject(jtodo, "priority", 1);       /* 默认中优先级 */
            cJSON_AddNumberToObject(jtodo, "due_date", 0);      /* 无截止日期 */
            cJSON_AddNumberToObject(jtodo, "group_id", 0);       /* 未分组 */
            cJSON_AddNumberToObject(jtodo, "sort_order", i);     /* 按原顺序 */

            cJSON *jca = cJSON_GetObjectItem(jissue, "created_at");
            cJSON_AddItemReferenceToObject(jtodo, "created_at", jca ? jca : cJSON_CreateNumber(now_ts()));

            cJSON *jua = cJSON_GetObjectItem(jissue, "updated_at");
            cJSON_AddItemReferenceToObject(jtodo, "updated_at", jua ? jua : cJSON_CreateNumber(now_ts()));

            cJSON_AddItemToArray(jtodos, jtodo);
        }
    }
    cJSON_AddItemToObject(new_root, "todos", jtodos);

    /* 迁移 checklist（issue_id → todo_id） */
    cJSON *jchecks = cJSON_GetObjectItem(root, "checklist");
    cJSON *jnew_checks = cJSON_CreateArray();

    if (jchecks && cJSON_IsArray(jchecks)) {
        int n = cJSON_GetArraySize(jchecks);
        for (int i = 0; i < n; i++) {
            cJSON *jcheck = cJSON_GetArrayItem(jchecks, i);
            cJSON *jnew_check = cJSON_CreateObject();

            cJSON *jid = cJSON_GetObjectItem(jcheck, "id");
            cJSON_AddItemReferenceToObject(jnew_check, "id", jid ? jid : cJSON_CreateNumber(0));

            /* issue_id → todo_id */
            cJSON *jiss = cJSON_GetObjectItem(jcheck, "issue_id");
            if (jiss) {
                cJSON_AddItemReferenceToObject(jnew_check, "todo_id", jiss);
            } else {
                cJSON_AddNumberToObject(jnew_check, "todo_id", 0);
            }

            cJSON *jtext = cJSON_GetObjectItem(jcheck, "text");
            cJSON_AddItemReferenceToObject(jnew_check, "text", jtext ? jtext : cJSON_CreateString(""));

            cJSON *jdone = cJSON_GetObjectItem(jcheck, "done");
            cJSON_AddItemReferenceToObject(jnew_check, "done", jdone ? jdone : cJSON_CreateBool(0));

            cJSON *jso = cJSON_GetObjectItem(jcheck, "sort_order");
            cJSON_AddItemReferenceToObject(jnew_check, "sort_order", jso ? jso : cJSON_CreateNumber(i));

            cJSON_AddItemToArray(jnew_checks, jnew_check);
        }
    }
    cJSON_AddItemToObject(new_root, "checklist", jnew_checks);

    /* 新增 groups 和 comments */
    cJSON_AddItemToObject(new_root, "groups", cJSON_CreateArray());
    cJSON_AddItemToObject(new_root, "comments", cJSON_CreateArray());

    /* 写入新文件 */
    char *out = cJSON_Print(new_root);
    cJSON_Delete(new_root);
    cJSON_Delete(root);

    if (!out) {
        fprintf(stderr, "[迁移] JSON 序列化失败\n");
        return -1;
    }

    FILE *fout = fopen(NEW_FILE, "w");
    if (!fout) {
        fprintf(stderr, "[迁移] 无法创建新数据文件: %s\n", NEW_FILE);
        free(out);
        return -1;
    }
    fputs(out, fout);
    fclose(fout);
    free(out);

    /* 备份旧文件 */
    if (remove(BACKUP_FILE) == 0) {
        fprintf(stderr, "[迁移] 已删除旧备份: %s\n", BACKUP_FILE);
    }
    if (rename(LEGACY_FILE, BACKUP_FILE) != 0) {
        fprintf(stderr, "[迁移] 警告: 无法备份旧文件\n");
        /* 不算失败，旧文件还在 */
    } else {
        printf("[迁移] 旧数据已迁移完成: %s → %s，备份: %s\n", LEGACY_FILE, NEW_FILE, BACKUP_FILE);
    }

    return 1;  /* 迁移发生 */
}

/* ============================================================
 * 公共接口
 * ============================================================ */
int todo_migrate_from_legacy(void) {
    /* 检查旧文件是否存在 */
    FILE *fp = fopen(LEGACY_FILE, "r");
    if (!fp) {
        printf("[迁移] 未发现旧数据文件 %s，无需迁移\n", LEGACY_FILE);
        return 0;  /* 无需迁移 */
    }
    fclose(fp);

    printf("[迁移] 发现旧数据文件 %s，开始迁移...\n", LEGACY_FILE);

    int ret = migrate_data();
    if (ret < 0) {
        fprintf(stderr, "[迁移] 迁移失败，请检查数据文件\n");
        return -1;
    }

    printf("[迁移] 完成，共迁移 1 个数据文件\n");
    return ret;  /* 0=无需迁移，1=迁移发生 */
}
