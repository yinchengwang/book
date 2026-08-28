 /**
  * seed_reading_radar.c
  * 
  * 将 reading-radar 数据迁移到 book.db
  * 
  * 用法:
  *   seed_reading_radar <book.db> <reading-radar-data-dir>
  * 
  * 1. 扫描 data/interview-questions/**/*.md 写入 interview_questions
  * 2. 扫描 data/interview-tracker/_index.json 写入 interview_tracker + interview_rounds
  * 3. 扫描 data/quiz/questions/**/*.js 解析题目写入 quiz_questions
  * 4. 扫描 data/excerpt/**/*.md + data/excerpt-meta.json 写入 notes_meta
  */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <time.h>

#define PATH_MAX_LEN 4096
#define LINE_MAX 16384

static sqlite3 *g_db = NULL;
static int g_total_quiz = 0;
static int g_total_interview_q = 0;
static int g_total_interview_tracker = 0;
static int g_total_excerpt = 0;

/* ========== SQLite helpers ========== */

static int db_open_seed(const char *path) {
    return sqlite3_open(path, &g_db);
}

static void db_close_seed(void) {
    if (g_db) sqlite3_close(g_db);
}

static int db_exec_seed(const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "  [seed] SQL error: %s\n  SQL: %s\n", err, sql);
        sqlite3_free(err);
    }
    return rc;
}

/* ========== Create quiz tables ========== */

static void create_tables(void) {
    db_exec_seed(
        "CREATE TABLE IF NOT EXISTS quiz_questions ("
        "  id TEXT PRIMARY KEY, stack TEXT NOT NULL, title TEXT NOT NULL,"
        "  category TEXT NOT NULL DEFAULT '', difficulty TEXT NOT NULL DEFAULT '',"
        "  options TEXT, answer TEXT NOT NULL, explanation TEXT,"
        "  tags TEXT NOT NULL DEFAULT '[]', time_estimate INTEGER DEFAULT 0,"
        "  created_at TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS quiz_answers ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  question_id TEXT NOT NULL, user_answer TEXT NOT NULL,"
        "  correct INTEGER NOT NULL DEFAULT 0,"
        "  date TEXT NOT NULL, timestamp INTEGER NOT NULL,"
        "  time_spent INTEGER DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS interview_questions ("
        "  id TEXT PRIMARY KEY, category TEXT NOT NULL, title TEXT NOT NULL,"
        "  body TEXT NOT NULL, tags TEXT NOT NULL DEFAULT '[]',"
        "  created_at TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS interview_tracker ("
        "  id TEXT PRIMARY KEY, company TEXT NOT NULL, position TEXT,"
        "  status TEXT NOT NULL DEFAULT 'screening', salary TEXT, notes TEXT,"
        "  created_at TEXT NOT NULL, updated_at TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS interview_rounds ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  tracker_id TEXT NOT NULL, round_type TEXT NOT NULL,"
        "  round_date TEXT NOT NULL, content TEXT, created_at TEXT NOT NULL);"
    );
}

/* ========== 1. Scan quiz JS files ========== */

static void scan_quiz_dir(const char *dir_path, const char *stack) {
    DIR *d = opendir(dir_path);
    if (!d) { fprintf(stderr, "  [seed] cannot open quiz dir: %s\n", dir_path); return; }
    
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN) continue;
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len < 3 || strcmp(name + len - 3, ".js") != 0) continue;
        
        char full_path[PATH_MAX_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, name);
        
        FILE *f = fopen(full_path, "r");
        if (!f) { fprintf(stderr, "  [seed] cannot open: %s\n", full_path); continue; }
        
        /* 简易关键词检测: 如果包含 "c_json_parse" 则用 CJSON 解析，否则跳过
           这里简化——直接跳过，需要更完备的 JS->JSON 解析器 */
        printf("  [seed] quiz file (skip, needs JS parser): %s\n", name);
        fclose(f);
    }
    closedir(d);
}

static void migrate_quiz(const char *data_dir) {
    printf("[seed] Migrating quiz questions...\n");
    const char *stacks[] = {"c", "cpp", "db", "ds", "linux", "py", "vdb", NULL};
    char quiz_dir[PATH_MAX_LEN];
    
    for (int i = 0; stacks[i]; i++) {
        snprintf(quiz_dir, sizeof(quiz_dir), "%s/data/quiz/questions/%s", data_dir, stacks[i]);
        struct stat st;
        if (stat(quiz_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
            scan_quiz_dir(quiz_dir, stacks[i]);
        }
    }
    printf("[seed] Quiz questions migrated: %d\n", g_total_quiz);
}

/* ========== 2. Migrate interview questions from .md files ========== */

static void scan_interview_md(const char *dir_path, const char *category) {
    DIR *d = opendir(dir_path);
    if (!d) return;
    
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            char subdir[PATH_MAX_LEN];
            snprintf(subdir, sizeof(subdir), "%s/%s", dir_path, entry->d_name);
            char subcat[256];
            snprintf(subcat, sizeof(subcat), "%s/%s", category, entry->d_name);
            scan_interview_md(subdir, subcat);
        } else if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) {
            const char *name = entry->d_name;
            size_t len = strlen(name);
            if (len < 3 || strcmp(name + len - 3, ".md") != 0) continue;
            
            /* 迁移: 解析 YAML frontmatter + body */
            char full_path[PATH_MAX_LEN];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, name);
            
            FILE *f = fopen(full_path, "r");
            if (!f) continue;
            
            char buf[65536];
            size_t total_read = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            buf[total_read] = '\0';
            
            /* 提取 frontmatter: ---\n...\n--- */
            char title[1024] = "";
            char difficulty[64] = "";
            char priority[64] = "";
            char tags[4096] = "[]";
            char body[65536] = "";
            
            const char *p = buf;
            if (strncmp(p, "---", 3) == 0) {
                p += 3;
                const char *end = strstr(p, "\n---");
                if (end) {
                    char front[8192];
                    size_t flen = end - p;
                    if (flen > sizeof(front) - 1) flen = sizeof(front) - 1;
                    strncpy(front, p, flen);
                    front[flen] = '\0';
                    
                    /* 简易 YAML 解析 */
                    char line[2048];
                    const char *lp = front;
                    while (*lp) {
                        const char *nl = strchr(lp, '\n');
                        if (!nl) nl = lp + strlen(lp);
                        size_t llen = nl - lp;
                        if (llen > sizeof(line) - 1) llen = sizeof(line) - 1;
                        strncpy(line, lp, llen);
                        line[llen] = '\0';
                        lp = (*nl) ? nl + 1 : nl;
                        
                        /* 去掉首尾空格 */
                        char *start = line;
                        while (*start == ' ' || *start == '\t' || *start == '\r') start++;
                        char *endp = start + strlen(start) - 1;
                        while (endp > start && (*endp == ' ' || *endp == '\t' || *endp == '\r')) endp--;
                        *(endp + 1) = '\0';
                        
                        if (strncmp(start, "title:", 6) == 0) {
                            const char *v = start + 6;
                            while (*v == ' ') v++;
                            if (*v == '"') { v++; const char *qe = strchr(v, '"'); if (qe) { size_t tl = qe - v; if (tl > sizeof(title) - 1) tl = sizeof(title) - 1; strncpy(title, v, tl); title[tl] = '\0'; v = qe + 1; } }
                            else { strncpy(title, v, sizeof(title) - 1); }
                        } else if (strncmp(start, "difficulty:", 11) == 0) {
                            const char *v = start + 11;
                            while (*v == ' ') v++;
                            strncpy(difficulty, v, sizeof(difficulty) - 1);
                        } else if (strncmp(start, "priority:", 9) == 0) {
                            const char *v = start + 9;
                            while (*v == ' ') v++;
                            strncpy(priority, v, sizeof(priority) - 1);
                        } else if (strncmp(start, "tags:", 5) == 0) {
                            /* tags 可能是数组或列表 */
                            const char *v = start + 5;
                            while (*v == ' ') v++;
                            /* 如果以 [ 开头，直接复制 */
                            if (*v == '[') {
                                strncpy(tags, v, sizeof(tags) - 1);
                            } else {
                                /* 转为 JSON 数组 */
                                char tmp[2048] = "[";
                                int first = 1;
                                const char *tp = v;
                                while (*tp) {
                                    if (*tp == ' ' || *tp == '\t' || *tp == '\r' || *tp == '\n' || *tp == ',') { tp++; continue; }
                                    if (*tp == '-') { tp++; continue; }
                                    if (*tp == '#') break;  /* comment */
                                    const char *te = tp;
                                    while (*te && *te != '\n' && *te != ',') te++;
                                    if (!first) strncat(tmp, ",", sizeof(tmp) - strlen(tmp) - 1);
                                    first = 0;
                                    strncat(tmp, "\"", sizeof(tmp) - strlen(tmp) - 1);
                                    size_t tlen = te - tp;
                                    if (tlen > 0) {
                                        char tbuf[256];
                                        strncpy(tbuf, tp, tlen > sizeof(tbuf)-1 ? sizeof(tbuf)-1 : tlen);
                                        tbuf[tlen > sizeof(tbuf)-1 ? sizeof(tbuf)-1 : tlen] = '\0';
                                        strncat(tmp, tbuf, sizeof(tmp) - strlen(tmp) - 1);
                                    }
                                    strncat(tmp, "\"", sizeof(tmp) - strlen(tmp) - 1);
                                    tp = (*te) ? te + 1 : te;
                                }
                                strncat(tmp, "]", sizeof(tmp) - strlen(tmp) - 1);
                                strncpy(tags, tmp, sizeof(tags) - 1);
                            }
                        }
                    }
                    
                    /* body 在 --- 之后 */
                    p = end + 4;
                    while (*p == '\n') p++;
                    strncpy(body, p, sizeof(body) - 1);
                }
            } else {
                /* 无 frontmatter，全部作为 body */
                strncpy(body, buf, sizeof(body) - 1);
                /* 用文件名作为 title */
                strncpy(title, name, sizeof(title) - 1);
                char *dot = strrchr(title, '.');
                if (dot) *dot = '\0';
            }
            
            /* 生成 ID */
            char id[128];
            snprintf(id, sizeof(id), "iq_%s_%s", category, name);
            /* 替换特殊字符 */
            for (char *c = id; *c; c++) {
                if (*c == '/' || *c == '\\' || *c == '.') *c = '_';
            }
            
            char created_at[32];
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            strftime(created_at, sizeof(created_at), "%Y-%m-%dT%H:%M:%S", tm);
            
            /* 写入 DB */
            sqlite3_stmt *stmt;
            const char *sql = "INSERT OR IGNORE INTO interview_questions "
                "(id, category, title, body, tags, created_at) VALUES (?1,?2,?3,?4,?5,?6)";
            if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, category, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, title, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 4, body, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 5, tags, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 6, created_at, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_DONE) {
                    g_total_interview_q++;
                }
                sqlite3_finalize(stmt);
            }
        }
    }
    closedir(d);
}

static void migrate_interview_questions(const char *data_dir) {
    printf("[seed] Migrating interview questions...\n");
    char dir[PATH_MAX_LEN];
    snprintf(dir, sizeof(dir), "%s/data/interview-questions", data_dir);
    scan_interview_md(dir, "");
    printf("[seed] Interview questions migrated: %d\n", g_total_interview_q);
}

/* ========== 3. Migrate interview tracker ========== */

static void migrate_interview_tracker(const char *data_dir) {
    printf("[seed] Migrating interview tracker...\n");
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/data/interview-tracker/_index.json", data_dir);
    
    FILE *f = fopen(path, "r");
    if (!f) { printf("  [seed] no _index.json found (skip)\n"); return; }
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = (char *)malloc(fsize + 1);
    if (!content) { fclose(f); return; }
    fread(content, 1, fsize, f);
    fclose(f);
    content[fsize] = '\0';
    
    /* 简单的基于关键字的解析——只提取 company 字段 */
    /* 实际上应该用 CJSON 解析，但这里简化 */
    const char *p = content;
    while ((p = strstr(p, "\"companyId\"")) != NULL) {
        p += 11; /* skip "companyId": */
        while (*p && *p != '"') p++;
        if (*p) p++;
        const char *id_start = p;
        while (*p && *p != '"') p++;
        if (!*p) break;
        char id[256];
        size_t id_len = p - id_start;
        if (id_len > sizeof(id) - 1) id_len = sizeof(id) - 1;
        strncpy(id, id_start, id_len);
        id[id_len] = '\0';
        
        /* scan for company name */
        const char *co = strstr(p, "\"company\"");
        if (!co) continue;
        co += 10;
        while (*co && *co != '"') co++;
        if (*co) co++;
        const char *co_start = co;
        while (*co && *co != '"') co++;
        char company[256];
        size_t co_len = co - co_start;
        if (co_len > sizeof(company) - 1) co_len = sizeof(company) - 1;
        strncpy(company, co_start, co_len);
        company[co_len] = '\0';
        
        /* scan for position */
        const char *po = strstr(co, "\"position\"");
        char position[256] = "";
        if (po) {
            po += 10;
            while (*po && *po != '"') po++;
            if (*po) po++;
            const char *po_start = po;
            while (*po && *po != '"') po++;
            size_t po_len = po - po_start;
            if (po_len > sizeof(position) - 1) po_len = sizeof(position) - 1;
            strncpy(position, po_start, po_len);
            position[po_len] = '\0';
        }
        
        char tracker_id[256];
        snprintf(tracker_id, sizeof(tracker_id), "tr_%s", id);
        
        char created_at[32];
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        strftime(created_at, sizeof(created_at), "%Y-%m-%dT%H:%M:%S", tm);
        
        sqlite3_stmt *stmt;
        const char *sql = "INSERT OR IGNORE INTO interview_tracker "
            "(id, company, position, status, created_at, updated_at) VALUES (?1,?2,?3,'applied',?4,?4)";
        if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, tracker_id, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, company, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, position, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, created_at, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_DONE) g_total_interview_tracker++;
            sqlite3_finalize(stmt);
        }
        
        p = co;
    }
    
    free(content);
    printf("[seed] Interview trackers migrated: %d\n", g_total_interview_tracker);
}

/* ========== 4. Migrate excerpts to notes_meta ========== */

static void migrate_excerpts(const char *data_dir) {
    printf("[seed] Migrating excerpts to notes_meta...\n");
    char dir[PATH_MAX_LEN];
    
    const char *years[] = {"2024", "2025", NULL};
    for (int yi = 0; years[yi]; yi++) {
        snprintf(dir, sizeof(dir), "%s/data/excerpt/%s", data_dir, years[yi]);
        struct stat st;
        if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        
        DIR *d = opendir(dir);
        if (!d) continue;
        
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN) continue;
            const char *name = entry->d_name;
            size_t len = strlen(name);
            if (len < 3 || strcmp(name + len - 3, ".md") != 0) continue;
            
            char full_path[PATH_MAX_LEN];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, name);
            
            FILE *f = fopen(full_path, "r");
            if (!f) continue;
            
            char buf[131072];
            size_t total = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            buf[total] = '\0';
            
            char title[1024] = "";
            char body[131072] = "";
            char tags[2048] = "[]";
            char context[32] = "excerpt";
            
            /* 简易 frontmatter 解析 */
            if (strncmp(buf, "---", 3) == 0) {
                const char *ep = buf + 3;
                const char *end = strstr(ep, "\n---");
                if (end) {
                    char front[8192];
                    size_t fl = end - ep;
                    if (fl > sizeof(front) - 1) fl = sizeof(front) - 1;
                    strncpy(front, ep, fl);
                    front[fl] = '\0';
                    
                    /* 提取 title */
                    const char *tl = strstr(front, "title:");
                    if (tl) {
                        tl += 6;
                        while (*tl == ' ') tl++;
                        const char *tle = strchr(tl, '\n');
                        size_t tllen = tle ? (size_t)(tle - tl) : strlen(tl);
                        if (tllen > sizeof(title) - 1) tllen = sizeof(title) - 1;
                        strncpy(title, tl, tllen);
                        title[tllen] = '\0';
                        if (title[0] == '"') { memmove(title, title + 1, strlen(title)); }
                        size_t ttl = strlen(title);
                        if (ttl > 0 && title[ttl - 1] == '"') title[ttl - 1] = '\0';
                    }
                    
                    body[0] = '\0';
                    p = end + 4;
                    while (*p == '\n') p++;
                    strncpy(body, p, sizeof(body) - 1);
                }
            } else {
                strncpy(body, buf, sizeof(body) - 1);
                strncpy(title, name, sizeof(title) - 1);
                char *dot = strrchr(title, '.');
                if (dot) *dot = '\0';
            }
            
            char id[128];
            char file_path[512];
            snprintf(id, sizeof(id), "ex_%s_%s", years[yi], name);
            for (char *cp = id; *cp; cp++) if (*cp == '.' || *cp == '/') *cp = '_';
            snprintf(file_path, sizeof(file_path), "excerpt/%s/%s", years[yi], name);
            
            char created_at[32];
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            strftime(created_at, sizeof(created_at), "%Y-%m-%dT%H:%M:%S", tm);
            
            sqlite3_stmt *stmt;
            const char *sql = "INSERT OR IGNORE INTO notes_meta "
                "(id, file_path, title, body, tags, context, source, created_at, updated_at) "
                "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?8)";
            if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, file_path, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, title, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 4, body, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 5, tags, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 6, context, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 7, name, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 8, created_at, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_DONE) g_total_excerpt++;
                sqlite3_finalize(stmt);
            }
        }
        closedir(d);
    }
    printf("[seed] Excerpts migrated: %d\n", g_total_excerpt);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: seed_reading_radar <book.db> <reading-radar-data-dir>\n");
        return 1;
    }
    
    printf("[seed] Opening database: %s\n", argv[1]);
    if (db_open_seed(argv[1]) != SQLITE_OK) {
        fprintf(stderr, "[seed] Failed to open: %s\n", argv[1]);
        return 1;
    }
    
    create_tables();
    
    migrate_quiz(argv[2]);
    migrate_interview_questions(argv[2]);
    migrate_interview_tracker(argv[2]);
    migrate_excerpts(argv[2]);
    
    printf("\n[seed] Migration summary:\n");
    printf("  Quiz questions:      %d (JS parsing needs Node.js helper)\n", g_total_quiz);
    printf("  Interview questions: %d\n", g_total_interview_q);
    printf("  Interview trackers:  %d\n", g_total_interview_tracker);
    printf("  Excerpts:            %d\n", g_total_excerpt);
    
    db_close_seed();
    return 0;
}
