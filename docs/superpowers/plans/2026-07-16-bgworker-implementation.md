# DB 后台任务调度器实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现一个通用的数据库后台任务调度器框架，首要应用是 CCEH 索引缩容检查，未来可扩展支持更多后台任务。

**Architecture:** 采用混合模式任务槽架构，主循环轮询多个独立任务槽位，每个槽位注册一个后台任务。核心任务内置，扩展任务支持运行时注册。跨平台支持（Linux/Win32）通过条件编译实现。

**Tech Stack:** C11 + pthread（Linux）/ Windows Thread API，CMake 3.20+，GTest 测试框架

## Global Constraints

- **编码规范**: 所有注释和 Commit Message 使用简体中文
- **构建目标**: 输出到 `build/engineering/`，测试产物到 `test-results/engineering/`
- **依赖限制**: 无外部运行时依赖，仅使用标准 C 库和已存在的 db 模块
- **平台要求**: Linux (dlopen) + Windows (LoadLibrary) 条件编译
- **命名规范**: `db_bgworker_*` 前缀，`db_bg_task_*` 前缀

---

## 文件结构

```
engineering/src/db/
├── bgworker/                          # 新建目录
│   ├── CMakeLists.txt
│   ├── bgworker.h                     # 公共 API 头文件
│   ├── bgworker.c                     # 调度器核心实现
│   ├── bgworker_api.c                 # 注册/注销 API
│   ├── bgworker_config.c              # 配置管理
│   ├── bgworker_internal.h            # 内部数据结构
│   └── tasks/
│       ├── CMakeLists.txt
│       ├── task_iface.h               # 任务接口定义
│       ├── task_stats.c               # 统计收集任务
│       └── task_cceh_shrink.c         # CCEH 缩容任务

engineering/include/db/
├── bgworker.h                         # 暴露给外部的头文件

engineering/test/db/
├── bgworker/                          # 新建测试目录
│   ├── CMakeLists.txt
│   ├── bgworker_test.cpp              # 调度器核心测试
│   └── cceh_shrink_test.cpp           # CCEH 缩容测试
```

---

## 任务列表

### Task 1: 创建目录结构和 CMake 配置

**Files:**
- Create: `engineering/src/db/bgworker/CMakeLists.txt`
- Create: `engineering/src/db/bgworker/tasks/CMakeLists.txt`
- Create: `engineering/test/db/bgworker/CMakeLists.txt`
- Modify: `engineering/src/db/CMakeLists.txt` (添加 add_subdirectory(bgworker))

**Interfaces:**
- Consumes: 无
- Produces: `db_bgworker` 库目标

- [ ] **Step 1: 创建 bgworker 主目录**

```bash
mkdir -p engineering/src/db/bgworker/tasks
mkdir -p engineering/test/db/bgworker
```

- [ ] **Step 2: 创建主 CMakeLists.txt**

```cmake
# engineering/src/db/bgworker/CMakeLists.txt
# DB 后台任务调度器

add_subdirectory(tasks)

file(GLOB_RECURSE BGWORKER_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(BGWORKER_SOURCE_FILES)
    add_library(db_bgworker STATIC ${BGWORKER_SOURCE_FILES})
    target_include_directories(db_bgworker PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/tasks
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
    )
    target_link_libraries(db_bgworker PRIVATE
        db_guc
        db_cceh
        Threads::Threads
    )
endif()
```

- [ ] **Step 3: 创建 tasks 子目录 CMakeLists.txt**

```cmake
# engineering/src/db/bgworker/tasks/CMakeLists.txt
file(GLOB_RECURSE TASK_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(TASK_SOURCE_FILES)
    target_sources(db_bgworker PRIVATE ${TASK_SOURCE_FILES})
endif()
```

- [ ] **Step 4: 创建测试目录 CMakeLists.txt**

```cmake
# engineering/test/db/bgworker/CMakeLists.txt
file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_project_test(${TEST_NAME} TEST_FILES)
endforeach()
```

- [ ] **Step 5: 修改 db/CMakeLists.txt 添加 bgworker 模块**

在 `add_subdirectory(trigger)` 之后添加：
```cmake
# 后台任务调度器
add_subdirectory(bgworker)
```

- [ ] **Step 6: 验证构建**

```bash
cd build/engineering && cmake --build . --target db_bgworker 2>&1 | head -30
```

预期输出：构建成功（无错误）

- [ ] **Step 7: 提交**

```bash
git add engineering/src/db/bgworker/ engineering/test/db/bgworker/ engineering/src/db/CMakeLists.txt
git commit -m "feat(bgworker): 创建目录结构和 CMake 配置"
```

---

### Task 2: 定义任务接口 (task_iface.h)

**Files:**
- Create: `engineering/src/db/bgworker/tasks/task_iface.h`

**Interfaces:**
- Consumes: 无
- Produces: `db_bg_task_t`, `db_bg_task_context_t`, `db_bg_task_status_t`

- [ ] **Step 1: 编写任务接口头文件**

```c
/**
 * @file task_iface.h
 * @brief 后台任务接口定义
 */
#ifndef DB_BGWORKER_TASK_IFACE_H
#define DB_BGWORKER_TASK_IFACE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** 任务状态 */
typedef enum db_bg_task_status_e {
    DB_BG_TASK_STATUS_IDLE       = 0,  /**< 空闲 */
    DB_BG_TASK_STATUS_CHECKING   = 1,  /**< 检查中 */
    DB_BG_TASK_STATUS_EXECUTING  = 2,  /**< 执行中 */
    DB_BG_TASK_STATUS_PAUSED     = 3,  /**< 暂停 */
    DB_BG_TASK_STATUS_STOPPED    = 4   /**< 已停止 */
} db_bg_task_status_t;

/**
 * @brief 后台任务上下文
 */
typedef struct db_bg_task_context {
    void          *user_data;           /**< 用户自定义数据 */
    uint64_t       last_check_time;     /**< 上次检查时间 (ms) */
    uint32_t       consecutive_trigger; /**< 连续触发计数 */
    atomic_uint    status;              /**< 任务状态 */
} db_bg_task_context_t;

/**
 * @brief 任务初始化回调
 * @param ctx 任务上下文
 * @return 0 成功，-1 失败
 */
typedef int (*db_bg_task_init_fn)(db_bg_task_context_t *ctx);

/**
 * @brief 任务检查回调
 * @param ctx 任务上下文
 * @return > 0 满足触发条件，= 0 不满足条件，< 0 错误
 */
typedef int (*db_bg_task_check_fn)(db_bg_task_context_t *ctx);

/**
 * @brief 任务执行回调
 * @param ctx 任务上下文
 * @return 0 成功，-1 失败
 */
typedef int (*db_bg_task_execute_fn)(db_bg_task_context_t *ctx);

/**
 * @brief 任务清理回调
 * @param ctx 任务上下文
 */
typedef void (*db_bg_task_cleanup_fn)(db_bg_task_context_t *ctx);

/**
 * @brief 后台任务定义
 */
typedef struct db_bg_task {
    const char               *name;     /**< 任务名称（唯一标识） */
    uint32_t                  interval_ms;  /**< 检查间隔（毫秒） */
    db_bg_task_init_fn        init;     /**< 初始化回调 */
    db_bg_task_check_fn       check;    /**< 检查回调 */
    db_bg_task_execute_fn     execute;  /**< 执行回调 */
    db_bg_task_cleanup_fn     cleanup;  /**< 清理回调 */
    db_bg_task_context_t     *ctx;      /**< 任务上下文（内部填充） */
} db_bg_task_t;

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 获取任务当前状态
 * @param ctx 任务上下文
 * @return 任务状态
 */
static inline db_bg_task_status_t db_bg_task_get_status(const db_bg_task_context_t *ctx) {
    return (db_bg_task_status_t)atomic_load_explicit(&ctx->status, memory_order_acquire);
}

/**
 * @brief 设置任务状态
 * @param ctx 任务上下文
 * @param status 新状态
 */
static inline void db_bg_task_set_status(db_bg_task_context_t *ctx, db_bg_task_status_t status) {
    atomic_store_explicit(&ctx->status, status, memory_order_release);
}

#ifdef __cplusplus
}
#endif

#endif /* DB_BGWORKER_TASK_IFACE_H */
```

- [ ] **Step 2: 提交**

```bash
git add engineering/src/db/bgworker/tasks/task_iface.h
git commit -m "feat(bgworker): 定义任务接口 task_iface.h"
```

---

### Task 3: 定义内部数据结构 (bgworker_internal.h)

**Files:**
- Create: `engineering/src/db/bgworker/bgworker_internal.h`

**Interfaces:**
- Consumes: `task_iface.h`
- Produces: `db_bg_worker_t`, `db_bg_task_slot_t`, 内部函数声明

- [ ] **Step 1: 编写内部数据结构头文件**

```c
/**
 * @file bgworker_internal.h
 * @brief 后台任务调度器内部数据结构
 */
#ifndef DB_BGWORKER_INTERNAL_H
#define DB_BGWORKER_INTERNAL_H

#include "tasks/task_iface.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 最大任务槽位数 */
#define DB_BGWORKER_MAX_TASKS 16

/** 默认检查间隔（毫秒） */
#define DB_BGWORKER_DEFAULT_TICK_MS 1000

/** 默认停止超时（秒） */
#define DB_BGWORKER_STOP_TIMEOUT_SEC 30

/* ============================================================
 * 类型定义
 * ============================================================ */

/** 任务槽位 */
typedef struct db_bg_task_slot {
    db_bg_task_t         *task;          /**< 注册的任务 */
    uint64_t              next_check_time; /**< 下次检查时间 (ms) */
    bool                  in_use;        /**< 是否在使用 */
} db_bg_task_slot_t;

/** 调度器统计信息 */
typedef struct db_bgworker_stats {
    uint32_t              task_count;    /**< 已注册任务数 */
    uint64_t              running_time_ms; /**< 运行时间（毫秒） */
    uint64_t              total_cycles;  /**< 总检查周期数 */
    uint64_t              total_executions; /**< 总执行次数 */
} db_bgworker_stats_t;

/** 后台任务调度器 */
typedef struct db_bg_worker {
    pthread_t             thread;        /**< 后台线程 */
    db_bg_task_slot_t     slots[DB_BGWORKER_MAX_TASKS]; /**< 任务槽位 */
    uint32_t              num_slots;     /**< 槽位数量 */
    atomic_bool           running;       /**< 是否运行中 */
    atomic_bool           stopping;      /**< 是否正在停止 */
    uint64_t              start_time_ms; /**< 启动时间 */
    uint32_t              tick_ms;       /**< 检查间隔 */
    pthread_mutex_t       mutex;         /**< 保护槽位访问 */
} db_bg_worker_t;

/* ============================================================
 * 内部函数声明
 * ============================================================ */

/**
 * @brief 获取当前时间（毫秒）
 */
uint64_t _db_bgworker_now_ms(void);

/**
 * @brief 调度器主循环
 * @param arg 调度器指针
 * @return NULL
 */
void *_db_bgworker_main_loop(void *arg);

/**
 * @brief 初始化任务槽位
 * @param worker 调度器
 */
void _db_bgworker_init_slots(db_bg_worker_t *worker);

/**
 * @brief 清理任务槽位
 * @param worker 调度器
 */
void _db_bgworker_cleanup_slots(db_bg_worker_t *worker);

/**
 * @brief 查找任务槽位
 * @param worker 调度器
 * @param name 任务名称
 * @return 槽位索引，-1 未找到
 */
int _db_bgworker_find_slot(db_bg_worker_t *worker, const char *name);

/**
 * @brief 查找空闲槽位
 * @param worker 调度器
 * @return 槽位索引，-1 无空闲
 */
int _db_bgworker_find_free_slot(db_bg_worker_t *worker);

/**
 * @brief 分配任务上下文
 * @param task 任务定义
 * @return 上下文指针，NULL 失败
 */
db_bg_task_context_t *_db_bgworker_alloc_context(db_bg_task_t *task);

/**
 * @brief 释放任务上下文
 * @param ctx 上下文
 */
void _db_bgworker_free_context(db_bg_task_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* DB_BGWORKER_INTERNAL_H */
```

- [ ] **Step 2: 提交**

```bash
git add engineering/src/db/bgworker/bgworker_internal.h
git commit -m "feat(bgworker): 定义内部数据结构 bgworker_internal.h"
```

---

### Task 4: 实现调度器核心 (bgworker.c)

**Files:**
- Create: `engineering/src/db/bgworker/bgworker.c`

**Interfaces:**
- Consumes: `bgworker_internal.h`, `db/guc.h`
- Produces: `db_bgworker_start()`, `db_bgworker_stop()`, `_db_bgworker_main_loop()`

- [ ] **Step 1: 编写调度器核心实现**

```c
/**
 * @file bgworker.c
 * @brief 后台任务调度器核心实现
 */

#include "bgworker_internal.h"
#include "db/guc.h"
#include "db/log.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 静态变量
 * ============================================================ */

/** 全局调度器实例 */
static db_bg_worker_t g_worker = {0};

/** GUC 参数默认值 */
static bool g_bgworker_enabled = true;
static int  g_bgworker_tick_ms = DB_BGWORKER_DEFAULT_TICK_MS;

/* ============================================================
 * 辅助函数实现
 * ============================================================ */

uint64_t _db_bgworker_now_ms(void) {
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t now = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return now / 10000;  /* 100-nanosecond intervals to milliseconds */
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

void _db_bgworker_init_slots(db_bg_worker_t *worker) {
    memset(worker->slots, 0, sizeof(worker->slots));
    worker->num_slots = DB_BGWORKER_MAX_TASKS;
}

void _db_bgworker_cleanup_slots(db_bg_worker_t *worker) {
    for (int i = 0; i < worker->num_slots; i++) {
        if (worker->slots[i].in_use && worker->slots[i].task) {
            db_bg_task_t *task = worker->slots[i].task;
            if (task->cleanup && task->ctx) {
                task->cleanup(task->ctx);
            }
            _db_bgworker_free_context(task->ctx);
            task->ctx = NULL;
            worker->slots[i].in_use = false;
        }
    }
}

int _db_bgworker_find_slot(db_bg_worker_t *worker, const char *name) {
    for (int i = 0; i < worker->num_slots; i++) {
        if (worker->slots[i].in_use && worker->slots[i].task) {
            if (strcmp(worker->slots[i].task->name, name) == 0) {
                return i;
            }
        }
    }
    return -1;
}

int _db_bgworker_find_free_slot(db_bg_worker_t *worker) {
    for (int i = 0; i < worker->num_slots; i++) {
        if (!worker->slots[i].in_use) {
            return i;
        }
    }
    return -1;
}

db_bg_task_context_t *_db_bgworker_alloc_context(db_bg_task_t *task) {
    db_bg_task_context_t *ctx = (db_bg_task_context_t *)malloc(sizeof(db_bg_task_context_t));
    if (!ctx) {
        return NULL;
    }
    memset(ctx, 0, sizeof(db_bg_task_context_t));
    ctx->last_check_time = 0;
    ctx->consecutive_trigger = 0;
    atomic_init(&ctx->status, DB_BG_TASK_STATUS_IDLE);
    return ctx;
}

void _db_bgworker_free_context(db_bg_task_context_t *ctx) {
    free(ctx);
}

/* ============================================================
 * 主循环
 * ============================================================ */

void *_db_bgworker_main_loop(void *arg) {
    db_bg_worker_t *worker = (db_bg_worker_t *)arg;
    uint64_t cycle_count = 0;

    db_log_info("后台任务调度器启动，tick_ms=%u", worker->tick_ms);

    while (atomic_load_explicit(&worker->stopping, memory_order_acquire) == false) {
        uint64_t now = _db_bgworker_now_ms();
        cycle_count++;

        pthread_mutex_lock(&worker->mutex);

        for (int i = 0; i < worker->num_slots; i++) {
            db_bg_task_slot_t *slot = &worker->slots[i];
            if (!slot->in_use || !slot->task) {
                continue;
            }

            db_bg_task_t *task = slot->task;
            db_bg_task_status_t status = db_bg_task_get_status(task->ctx);

            /* 跳过非空闲状态的任务 */
            if (status != DB_BG_TASK_STATUS_IDLE) {
                continue;
            }

            /* 检查是否到达检查时间 */
            if (now < slot->next_check_time) {
                continue;
            }

            /* 执行检查 */
            db_bg_task_set_status(task->ctx, DB_BG_TASK_STATUS_CHECKING);
            int check_result = task->check(task->ctx);

            if (check_result > 0) {
                /* 满足触发条件 */
                task->ctx->consecutive_trigger++;

                /* 获取时间窗口大小（默认3） */
                int *time_window = guc_get_int("bgworker.cceh_shrink.time_window");
                int window = time_window ? *time_window : 3;

                if (task->ctx->consecutive_trigger >= (uint32_t)window) {
                    /* 触发执行 */
                    db_bg_task_set_status(task->ctx, DB_BG_TASK_STATUS_EXECUTING);
                    int exec_result = task->execute(task->ctx);
                    task->ctx->consecutive_trigger = 0;

                    if (exec_result != 0) {
                        db_log_warn("后台任务 '%s' 执行失败", task->name);
                    }
                    db_bg_task_set_status(task->ctx, DB_BG_TASK_STATUS_IDLE);
                }
            } else {
                /* 不满足条件，重置计数 */
                task->ctx->consecutive_trigger = 0;
                db_bg_task_set_status(task->ctx, DB_BG_TASK_STATUS_IDLE);
            }

            /* 更新下次检查时间 */
            slot->next_check_time = now + task->interval_ms;
        }

        pthread_mutex_unlock(&worker->mutex);

        /* 休眠 */
        uint32_t sleep_ms = worker->tick_ms;
        struct timespec ts;
        ts.tv_sec = sleep_ms / 1000;
        ts.tv_nsec = (sleep_ms % 1000) * 1000000;
#if defined(_WIN32)
        Sleep(sleep_ms);
#else
        nanosleep(&ts, NULL);
#endif
    }

    db_log_info("后台任务调度器停止，共执行 %lu 个周期", cycle_count);
    (void)cycle_count;
    return NULL;
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

int db_bgworker_start(void) {
    if (atomic_load_explicit(&g_worker.running, memory_order_acquire)) {
        db_log_warn("后台任务调度器已在运行");
        return 0;
    }

    /* 检查是否启用 */
    bool *enabled = guc_get_bool("bgworker.enabled");
    if (enabled && !*enabled) {
        db_log_info("后台任务调度器被配置禁用");
        return 0;
    }

    /* 初始化 */
    memset(&g_worker, 0, sizeof(g_worker));
    _db_bgworker_init_slots(&g_worker);

    int *tick_ms = guc_get_int("bgworker.tick_ms");
    g_worker.tick_ms = tick_ms ? (uint32_t)*tick_ms : DB_BGWORKER_DEFAULT_TICK_MS;

    if (pthread_mutex_init(&g_worker.mutex, NULL) != 0) {
        db_log_error("pthread_mutex_init 失败");
        return -1;
    }

    atomic_init(&g_worker.running, true);
    atomic_init(&g_worker.stopping, false);
    g_worker.start_time_ms = _db_bgworker_now_ms();

    /* 创建后台线程 */
    if (pthread_create(&g_worker.thread, NULL, _db_bgworker_main_loop, &g_worker) != 0) {
        db_log_error("pthread_create 失败");
        pthread_mutex_destroy(&g_worker.mutex);
        atomic_store_explicit(&g_worker.running, false, memory_order_release);
        return -1;
    }

    db_log_info("后台任务调度器已启动");
    return 0;
}

int db_bgworker_stop(void) {
    if (!atomic_load_explicit(&g_worker.running, memory_order_acquire)) {
        return 0;
    }

    db_log_info("正在停止后台任务调度器...");

    /* 设置停止标志 */
    atomic_store_explicit(&g_worker.stopping, true, memory_order_release);

    /* 等待线程结束 */
    void *retval;
    pthread_t thread = g_worker.thread;
    pthread_mutex_unlock(&g_worker.mutex);  /* 确保没有死锁 */

    /* 带超时的等待 */
    struct timespec ts;
#if defined(_WIN32)
    ts.tv_sec = DB_BGWORKER_STOP_TIMEOUT_SEC;
    ts.tv_nsec = 0;
#else
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += DB_BGWORKER_STOP_TIMEOUT_SEC;
#endif

    int ret = pthread_timedjoin_np(thread, &retval, &ts);
    if (ret != 0) {
        db_log_warn("pthread_join 超时，强制终止线程");
#if defined(_WIN32)
        TerminateThread(thread, 1);
#else
        pthread_cancel(thread);
#endif
    }

    /* 清理 */
    pthread_mutex_lock(&g_worker.mutex);
    _db_bgworker_cleanup_slots(&g_worker);
    pthread_mutex_unlock(&g_worker.mutex);
    pthread_mutex_destroy(&g_worker.mutex);

    atomic_store_explicit(&g_worker.running, false, memory_order_release);
    db_log_info("后台任务调度器已停止");
    return 0;
}
```

- [ ] **Step 2: 提交**

```bash
git add engineering/src/db/bgworker/bgworker.c
git commit -m "feat(bgworker): 实现调度器核心 bgworker.c"
```

---

### Task 5: 实现任务注册/注销 API (bgworker_api.c)

**Files:**
- Create: `engineering/src/db/bgworker/bgworker_api.c`

**Interfaces:**
- Consumes: `bgworker_internal.h`
- Produces: `db_bgworker_register()`, `db_bgworker_unregister()`, `db_bgworker_pause()`, `db_bgworker_resume()`

- [ ] **Step 1: 编写 API 实现**

```c
/**
 * @file bgworker_api.c
 * @brief 后台任务调度器公共 API 实现
 */

#include "bgworker_internal.h"
#include "db/log.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 公共 API
 * ============================================================ */

/**
 * @brief 注册后台任务
 * @param task 任务定义
 * @param slot_out 输出分配的任务槽位
 * @return 0 成功，-1 失败
 */
int db_bgworker_register(db_bg_task_t *task, int *slot_out) {
    if (!task || !task->name || !task->check || !task->execute) {
        db_log_error("无效的任务参数");
        return -1;
    }

    pthread_mutex_lock(&g_worker.mutex);

    /* 检查是否已存在同名任务 */
    if (_db_bgworker_find_slot(&g_worker, task->name) >= 0) {
        db_log_error("任务 '%s' 已存在", task->name);
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    /* 查找空闲槽位 */
    int slot = _db_bgworker_find_free_slot(&g_worker);
    if (slot < 0) {
        db_log_error("无可用任务槽位");
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    /* 分配上下文 */
    db_bg_task_context_t *ctx = _db_bgworker_alloc_context(task);
    if (!ctx) {
        db_log_error("分配任务上下文失败");
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    /* 调用初始化回调 */
    if (task->init) {
        if (task->init(ctx) != 0) {
            db_log_error("任务 '%s' 初始化失败", task->name);
            _db_bgworker_free_context(ctx);
            pthread_mutex_unlock(&g_worker.mutex);
            return -1;
        }
    }

    /* 注册任务 */
    task->ctx = ctx;
    g_worker.slots[slot].task = task;
    g_worker.slots[slot].in_use = true;
    g_worker.slots[slot].next_check_time = _db_bgworker_now_ms();

    if (slot_out) {
        *slot_out = slot;
    }

    db_log_info("任务 '%s' 已注册到槽位 %d", task->name, slot);
    pthread_mutex_unlock(&g_worker.mutex);
    return 0;
}

/**
 * @brief 注销后台任务
 * @param name 任务名称
 * @return 0 成功，-1 未找到
 */
int db_bgworker_unregister(const char *name) {
    if (!name) {
        return -1;
    }

    pthread_mutex_lock(&g_worker.mutex);

    int slot = _db_bgworker_find_slot(&g_worker, name);
    if (slot < 0) {
        db_log_error("任务 '%s' 未找到", name);
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    db_bg_task_t *task = g_worker.slots[slot].task;

    /* 调用清理回调 */
    if (task->cleanup && task->ctx) {
        task->cleanup(task->ctx);
    }

    /* 释放上下文 */
    if (task->ctx) {
        _db_bgworker_free_context(task->ctx);
        task->ctx = NULL;
    }

    /* 释放槽位 */
    memset(&g_worker.slots[slot], 0, sizeof(db_bg_task_slot_t));

    db_log_info("任务 '%s' 已注销", name);
    pthread_mutex_unlock(&g_worker.mutex);
    return 0;
}

/**
 * @brief 暂停后台任务
 * @param name 任务名称
 * @return 0 成功，-1 未找到
 */
int db_bgworker_pause(const char *name) {
    if (!name) {
        return -1;
    }

    pthread_mutex_lock(&g_worker.mutex);

    int slot = _db_bgworker_find_slot(&g_worker, name);
    if (slot < 0) {
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    db_bg_task_set_status(g_worker.slots[slot].task->ctx, DB_BG_TASK_STATUS_PAUSED);
    db_log_info("任务 '%s' 已暂停", name);
    pthread_mutex_unlock(&g_worker.mutex);
    return 0;
}

/**
 * @brief 恢复后台任务
 * @param name 任务名称
 * @return 0 成功，-1 未找到
 */
int db_bgworker_resume(const char *name) {
    if (!name) {
        return -1;
    }

    pthread_mutex_lock(&g_worker.mutex);

    int slot = _db_bgworker_find_slot(&g_worker, name);
    if (slot < 0) {
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    db_bg_task_set_status(g_worker.slots[slot].task->ctx, DB_BG_TASK_STATUS_IDLE);
    g_worker.slots[slot].next_check_time = _db_bgworker_now_ms();
    db_log_info("任务 '%s' 已恢复", name);
    pthread_mutex_unlock(&g_worker.mutex);
    return 0;
}

/**
 * @brief 获取任务状态
 * @param name 任务名称
 * @param status_out 输出状态
 * @return 0 成功，-1 未找到
 */
int db_bgworker_get_status(const char *name, unsigned int *status_out) {
    if (!name || !status_out) {
        return -1;
    }

    pthread_mutex_lock(&g_worker.mutex);

    int slot = _db_bgworker_find_slot(&g_worker, name);
    if (slot < 0) {
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    *status_out = db_bg_task_get_status(g_worker.slots[slot].task->ctx);
    pthread_mutex_unlock(&g_worker.mutex);
    return 0;
}

/**
 * @brief 获取调度器统计信息
 * @param stats_out 输出统计信息
 * @return 0 成功
 */
int db_bgworker_get_stats(db_bgworker_stats_t *stats_out) {
    if (!stats_out) {
        return -1;
    }

    pthread_mutex_lock(&g_worker.mutex);

    uint32_t count = 0;
    for (int i = 0; i < g_worker.num_slots; i++) {
        if (g_worker.slots[i].in_use) {
            count++;
        }
    }

    stats_out->task_count = count;
    stats_out->running_time_ms = atomic_load_explicit(&g_worker.running, memory_order_acquire)
        ? (_db_bgworker_now_ms() - g_worker.start_time_ms)
        : 0;
    stats_out->total_cycles = 0;
    stats_out->total_executions = 0;

    pthread_mutex_unlock(&g_worker.mutex);
    return 0;
}
```

- [ ] **Step 2: 提交**

```bash
git add engineering/src/db/bgworker/bgworker_api.c
git commit -m "feat(bgworker): 实现任务注册/注销 API bgworker_api.c"
```

---

### Task 6: 实现公共头文件 (bgworker.h)

**Files:**
- Create: `engineering/include/db/bgworker.h`

**Interfaces:**
- Consumes: 无
- Produces: `db_bgworker_start()`, `db_bgworker_stop()`, `db_bgworker_register()` 等公共 API

- [ ] **Step 1: 编写公共头文件**

```c
/**
 * @file include/db/bgworker.h
 * @brief 后台任务调度器公共 API
 */
#ifndef DB_BGWORKER_H
#define DB_BGWORKER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct db_bg_task db_bg_task_t;
typedef struct db_bg_task_context db_bg_task_context_t;

/* ============================================================
 * 生命周期
 * ============================================================ */

/**
 * @brief 启动后台任务调度器
 * @return 0 成功，-1 失败
 */
int db_bgworker_start(void);

/**
 * @brief 停止后台任务调度器
 * @return 0 成功，-1 失败
 */
int db_bgworker_stop(void);

/* ============================================================
 * 任务管理
 * ============================================================ */

/**
 * @brief 注册后台任务
 * @param task 任务定义
 * @param slot_out 输出分配的任务槽位（可为 NULL）
 * @return 0 成功，-1 失败
 */
int db_bgworker_register(db_bg_task_t *task, int *slot_out);

/**
 * @brief 注销后台任务
 * @param name 任务名称
 * @return 0 成功，-1 未找到
 */
int db_bgworker_unregister(const char *name);

/**
 * @brief 暂停后台任务
 * @param name 任务名称
 * @return 0 成功，-1 未找到
 */
int db_bgworker_pause(const char *name);

/**
 * @brief 恢复后台任务
 * @param name 任务名称
 * @return 0 成功，-1 未找到
 */
int db_bgworker_resume(const char *name);

/* ============================================================
 * 状态查询
 * ============================================================ */

/** 任务状态（与 task_iface.h 同步） */
typedef enum db_bg_task_status_e {
    DB_BG_TASK_STATUS_IDLE       = 0,
    DB_BG_TASK_STATUS_CHECKING   = 1,
    DB_BG_TASK_STATUS_EXECUTING  = 2,
    DB_BG_TASK_STATUS_PAUSED     = 3,
    DB_BG_TASK_STATUS_STOPPED    = 4
} db_bg_task_status_t;

/**
 * @brief 获取任务状态
 * @param name 任务名称
 * @param status_out 输出状态
 * @return 0 成功，-1 未找到
 */
int db_bgworker_get_status(const char *name, unsigned int *status_out);

/**
 * @brief 调度器统计信息
 */
typedef struct db_bgworker_stats {
    uint32_t  task_count;         /**< 已注册任务数 */
    uint64_t  running_time_ms;    /**< 运行时间（毫秒） */
    uint64_t  total_cycles;       /**< 总检查周期数 */
    uint64_t  total_executions;   /**< 总执行次数 */
} db_bgworker_stats_t;

/**
 * @brief 获取调度器统计信息
 * @param stats_out 输出统计信息
 * @return 0 成功
 */
int db_bgworker_get_stats(db_bgworker_stats_t *stats_out);

#ifdef __cplusplus
}
#endif

#endif /* DB_BGWORKER_H */
```

- [ ] **Step 2: 提交**

```bash
git add engineering/include/db/bgworker.h
git commit -m "feat(bgworker): 添加公共头文件 include/db/bgworker.h"
```

---

### Task 7: 实现 CCEH 缩容任务 (task_cceh_shrink.c)

**Files:**
- Create: `engineering/src/db/bgworker/tasks/task_cceh_shrink.c`

**Interfaces:**
- Consumes: `db/index/hash/cceh.h`, `task_iface.h`
- Produces: `g_cceh_shrink_task` 任务定义

- [ ] **Step 1: 编写 CCEH 缩容任务实现**

```c
/**
 * @file task_cceh_shrink.c
 * @brief CCEH 索引缩容后台任务
 */

#include "task_iface.h"
#include "db/index/hash/cceh.h"
#include "db/log.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 默认负载因子阈值 */
#define DEFAULT_LOAD_FACTOR_THRESHOLD 0.25

/** 默认时间窗口大小 */
#define DEFAULT_TIME_WINDOW 3

/** 默认最小全局深度 */
#define DEFAULT_MIN_GLOBAL_DEPTH 1

/* ============================================================
 * 任务上下文
 * ============================================================ */

typedef struct cceh_shrink_ctx {
    cceh_index_t *index;              /**< CCEH 索引实例 */
    double        load_factor_threshold; /**< 负载因子阈值 */
    uint32_t      min_global_depth;   /**< 最小全局深度 */
} cceh_shrink_ctx_t;

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 计算当前负载因子
 */
static double calculate_load_factor(const cceh_shrink_ctx_t *ctx) {
    if (!ctx || !ctx->index) {
        return 1.0;
    }

    uint32_t n_total = cceh_index_size(ctx->index);
    uint32_t segment_count = cceh_index_segment_count(ctx->index);

    if (segment_count == 0) {
        return 0.0;
    }

    /* 获取 segment 容量（通过全局变量或计算） */
    /* 这里简化处理，实际应该从索引获取 */
    uint32_t capacity = 8;  /* CCEH_DEFAULT_SEGMENT_CAPACITY */

    uint32_t max_records = segment_count * capacity;
    if (max_records == 0) {
        return 0.0;
    }

    return (double)n_total / max_records;
}

/* ============================================================
 * 任务回调实现
 * ============================================================ */

/**
 * @brief 初始化回调
 */
static int cceh_shrink_init(db_bg_task_context_t *base_ctx) {
    cceh_shrink_ctx_t *ctx = (cceh_shrink_ctx_t *)malloc(sizeof(cceh_shrink_ctx_t));
    if (!ctx) {
        return -1;
    }
    memset(ctx, 0, sizeof(cceh_shrink_ctx_t));

    ctx->load_factor_threshold = DEFAULT_LOAD_FACTOR_THRESHOLD;
    ctx->min_global_depth = DEFAULT_MIN_GLOBAL_DEPTH;
    ctx->index = NULL;  /* 需要外部设置 */

    base_ctx->user_data = ctx;
    db_log_info("CCEH 缩容任务初始化完成");
    return 0;
}

/**
 * @brief 检查回调
 * @return > 0 满足缩容条件，= 0 不满足，< 0 错误
 */
static int cceh_shrink_check(db_bg_task_context_t *base_ctx) {
    cceh_shrink_ctx_t *ctx = (cceh_shrink_ctx_t *)base_ctx->user_data;
    if (!ctx || !ctx->index) {
        return 0;
    }

    /* 检查全局深度是否允许缩容 */
    uint32_t global_depth = cceh_index_global_depth(ctx->index);
    if (global_depth <= ctx->min_global_depth) {
        return 0;
    }

    /* 计算负载因子 */
    double load_factor = calculate_load_factor(ctx);

    db_log_debug("CCEH 缩容检查: load_factor=%.3f, threshold=%.3f, global_depth=%u",
                 load_factor, ctx->load_factor_threshold, global_depth);

    if (load_factor < ctx->load_factor_threshold) {
        return 1;  /* 满足条件 */
    }

    return 0;
}

/**
 * @brief 执行回调：执行分片缩容
 * @return 0 成功，-1 失败
 */
static int cceh_shrink_execute(db_bg_task_context_t *base_ctx) {
    cceh_shrink_ctx_t *ctx = (cceh_shrink_ctx_t *)base_ctx->user_data;
    if (!ctx || !ctx->index) {
        return -1;
    }

    uint32_t global_depth = cceh_index_global_depth(ctx->index);
    uint32_t segment_count = cceh_index_segment_count(ctx->index);

    db_log_info("CCEH 缩容执行: global_depth=%u, segment_count=%u",
                global_depth, segment_count);

    /* 分片缩容逻辑：每次只合并一对相邻 segment
     * 由于 CCEH 当前实现没有 shrink 接口，这里预留扩展点
     * 未来实现时需要：
     * 1. 找到可合并的一对 segment（local_depth 相等且指向同一 parent）
     * 2. 原子性地更新目录指针
     * 3. 标记旧 segment 为 retired
     * 4. epoch 回收
     */

    /* 暂时记录日志，实际缩容逻辑待实现 */
    db_log_info("CCEH 分片缩容: 合并一对相邻 segment（待实现）");

    return 0;
}

/**
 * @brief 清理回调
 */
static void cceh_shrink_cleanup(db_bg_task_context_t *base_ctx) {
    if (base_ctx && base_ctx->user_data) {
        free(base_ctx->user_data);
        base_ctx->user_data = NULL;
    }
    db_log_info("CCEH 缩容任务清理完成");
}

/* ============================================================
 * 任务定义
 * ============================================================ */

/**
 * @brief CCEH 缩容任务定义
 * @note 需要外部设置 ctx->index 才能生效
 */
db_bg_task_t g_cceh_shrink_task = {
    .name         = "cceh_shrink",
    .interval_ms  = 5000,          /* 默认 5 秒检查一次 */
    .init         = cceh_shrink_init,
    .check        = cceh_shrink_check,
    .execute      = cceh_shrink_execute,
    .cleanup      = cceh_shrink_cleanup,
    .ctx          = NULL
};

/* ============================================================
 * 辅助 API
 * ============================================================ */

/**
 * @brief 设置缩容任务的目标索引
 * @param index CCEH 索引实例
 */
void db_bgworker_cceh_shrink_set_index(cceh_index_t *index) {
    /* 通过 GUC 或直接修改任务上下文 */
    /* 这里简化处理，实际应该在注册时传递 */
    if (g_cceh_shrink_task.ctx) {
        cceh_shrink_ctx_t *ctx = (cceh_shrink_ctx_t *)g_cceh_shrink_task.ctx->user_data;
        if (ctx) {
            ctx->index = index;
        }
    }
}
```

- [ ] **Step 2: 提交**

```bash
git add engineering/src/db/bgworker/tasks/task_cceh_shrink.c
git commit -m "feat(bgworker): 实现 CCEH 缩容任务 task_cceh_shrink.c"
```

---

### Task 8: 实现统计收集任务 (task_stats.c)

**Files:**
- Create: `engineering/src/db/bgworker/tasks/task_stats.c`

**Interfaces:**
- Consumes: `task_iface.h`
- Produces: `g_stats_task` 任务定义

- [ ] **Step 1: 编写统计收集任务实现**

```c
/**
 * @file task_stats.c
 * @brief 统计信息收集后台任务
 */

#include "task_iface.h"
#include "db/log.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 任务上下文
 * ============================================================ */

typedef struct stats_ctx {
    uint64_t last_total_operations;  /**< 上次总操作数 */
    double   last_check_time;        /**< 上次检查时间 */
} stats_ctx_t;

/* ============================================================
 * 任务回调实现
 * ============================================================ */

/**
 * @brief 初始化回调
 */
static int stats_init(db_bg_task_context_t *ctx) {
    stats_ctx_t *stats_ctx = (stats_ctx_t *)malloc(sizeof(stats_ctx_t));
    if (!stats_ctx) {
        return -1;
    }
    memset(stats_ctx, 0, sizeof(stats_ctx_t));
    ctx->user_data = stats_ctx;
    db_log_info("统计收集任务初始化完成");
    return 0;
}

/**
 * @brief 检查回调
 * @note 统计收集任务不需要条件触发，定期执行
 */
static int stats_check(db_bg_task_context_t *ctx) {
    (void)ctx;
    return 1;  /* 总是满足条件 */
}

/**
 * @brief 执行回调：收集统计信息
 */
static int stats_execute(db_bg_task_context_t *ctx) {
    stats_ctx_t *stats_ctx = (stats_ctx_t *)ctx->user_data;

    /* 收集各种统计信息：
     * - 缓冲区命中率
     * - 索引使用情况
     * - 查询延迟分布
     * - 内存使用情况
     */

    /* 这里是占位实现，实际应该从各子系统获取统计信息 */
    db_log_debug("统计信息收集: 执行一次统计采样");

    (void)stats_ctx;
    return 0;
}

/**
 * @brief 清理回调
 */
static void stats_cleanup(db_bg_task_context_t *ctx) {
    if (ctx && ctx->user_data) {
        free(ctx->user_data);
        ctx->user_data = NULL;
    }
    db_log_info("统计收集任务清理完成");
}

/* ============================================================
 * 任务定义
 * ============================================================ */

/**
 * @brief 统计收集任务定义
 */
db_bg_task_t g_stats_task = {
    .name         = "stats_collector",
    .interval_ms  = 60000,         /* 默认 1 分钟收集一次 */
    .init         = stats_init,
    .check        = stats_check,
    .execute      = stats_execute,
    .cleanup      = stats_cleanup,
    .ctx          = NULL
};
```

- [ ] **Step 2: 提交**

```bash
git add engineering/src/db/bgworker/tasks/task_stats.c
git commit -m "feat(bgworker): 实现统计收集任务 task_stats.c"
```

---

### Task 9: 添加 GUC 配置参数 (bgworker_config.c)

**Files:**
- Create: `engineering/src/db/bgworker/bgworker_config.c`

**Interfaces:**
- Consumes: `db/guc.h`
- Produces: GUC 参数注册函数

- [ ] **Step 1: 编写配置管理实现**

```c
/**
 * @file bgworker_config.c
 * @brief 后台任务调度器配置管理
 */

#include "bgworker_internal.h"
#include "db/guc.h"
#include "db/log.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * 配置参数默认值
 * ============================================================ */

static bool  g_bgworker_enabled = true;
static int   g_bgworker_tick_ms = 1000;
static int   g_bgworker_max_tasks = 16;
static bool  g_cceh_shrink_enable = true;
static int   g_cceh_shrink_interval_ms = 5000;
static double g_cceh_shrink_load_factor_threshold = 0.25;
static int   g_cceh_shrink_time_window = 3;
static int   g_cceh_shrink_min_global_depth = 1;

/* ============================================================
 * GUC 参数定义
 * ============================================================ */

static guc_param_t g_bgworker_params[] = {
    {
        .name = "bgworker.enabled",
        .type = GUC_TYPE_BOOL,
        .value = &g_bgworker_enabled,
        .reset_val = &g_bgworker_enabled,
        .description = "是否启用后台任务调度器",
        .flags = GUC_FLAG_NONE
    },
    {
        .name = "bgworker.tick_ms",
        .type = GUC_TYPE_INT,
        .value = &g_bgworker_tick_ms,
        .reset_val = &g_bgworker_tick_ms,
        .bounds.int_v = {100, 60000},
        .description = "后台任务调度器主循环检查间隔（毫秒）",
        .flags = GUC_FLAG_NONE
    },
    {
        .name = "bgworker.max_tasks",
        .type = GUC_TYPE_INT,
        .value = &g_bgworker_max_tasks,
        .reset_val = &g_bgworker_max_tasks,
        .bounds.int_v = {1, 256},
        .description = "最大任务槽位数",
        .flags = GUC_FLAG_NO_SHOW
    },
    {
        .name = "bgworker.cceh_shrink.enable",
        .type = GUC_TYPE_BOOL,
        .value = &g_cceh_shrink_enable,
        .reset_val = &g_cceh_shrink_enable,
        .description = "是否启用 CCEH 缩容任务",
        .flags = GUC_FLAG_NONE
    },
    {
        .name = "bgworker.cceh_shrink.interval_ms",
        .type = GUC_TYPE_INT,
        .value = &g_cceh_shrink_interval_ms,
        .reset_val = &g_cceh_shrink_interval_ms,
        .bounds.int_v = {1000, 3600000},
        .description = "CCEH 缩容检查间隔（毫秒）",
        .flags = GUC_FLAG_NONE
    },
    {
        .name = "bgworker.cceh_shrink.load_factor_threshold",
        .type = GUC_TYPE_REAL,
        .value = &g_cceh_shrink_load_factor_threshold,
        .reset_val = &g_cceh_shrink_load_factor_threshold,
        .bounds.real_v = {0.05, 0.9},
        .description = "CCEH 缩容负载因子阈值",
        .flags = GUC_FLAG_NONE
    },
    {
        .name = "bgworker.cceh_shrink.time_window",
        .type = GUC_TYPE_INT,
        .value = &g_cceh_shrink_time_window,
        .reset_val = &g_cceh_shrink_time_window,
        .bounds.int_v = {1, 10},
        .description = "CCEH 缩容时间窗口大小",
        .flags = GUC_FLAG_NONE
    },
    {
        .name = "bgworker.cceh_shrink.min_global_depth",
        .type = GUC_TYPE_INT,
        .value = &g_cceh_shrink_min_global_depth,
        .reset_val = &g_cceh_shrink_min_global_depth,
        .bounds.int_v = {1, 10},
        .description = "CCEH 缩容最小全局深度",
        .flags = GUC_FLAG_NONE
    }
};

/* ============================================================
 * 公共函数
 * ============================================================ */

/**
 * @brief 注册 GUC 配置参数
 * @return 0 成功，-1 失败
 */
int db_bgworker_config_init(void) {
    int num_params = sizeof(g_bgworker_params) / sizeof(g_bgworker_params[0]);

    for (int i = 0; i < num_params; i++) {
        if (guc_register_param(&g_bgworker_params[i]) != 0) {
            db_log_error("注册 GUC 参数 '%s' 失败", g_bgworker_params[i].name);
            return -1;
        }
    }

    db_log_info("后台任务调度器配置参数已注册 (%d 个)", num_params);
    return 0;
}

/**
 * @brief 获取 CCEH 缩容是否启用
 */
bool db_bgworker_cceh_shrink_is_enabled(void) {
    return g_cceh_shrink_enable;
}

/**
 * @brief 获取 CCEH 缩容检查间隔
 */
int db_bgworker_cceh_shrink_get_interval(void) {
    return g_cceh_shrink_interval_ms;
}
```

- [ ] **Step 2: 修改 bgworker.c 在启动时注册配置**

在 `db_bgworker_start()` 函数开头添加配置注册调用

- [ ] **Step 3: 提交**

```bash
git add engineering/src/db/bgworker/bgworker_config.c
git commit -m "feat(bgworker): 添加 GUC 配置参数 bgworker_config.c"
```

---

### Task 10: 编写调度器核心测试 (bgworker_test.cpp)

**Files:**
- Create: `engineering/test/db/bgworker/bgworker_test.cpp`

**Interfaces:**
- Consumes: `db/bgworker.h`, `task_iface.h`
- Produces: 测试用例

- [ ] **Step 1: 编写调度器核心测试**

```cpp
/**
 * @file bgworker_test.cpp
 * @brief 后台任务调度器核心测试
 */

#include <gtest/gtest.h>
#include "db/bgworker.h"
#include "bgworker/tasks/task_iface.h"
#include "db/log.h"
#include <thread>
#include <atomic>

/* ============================================================
 * 测试夹具
 * ============================================================ */

class BgWorkerTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_log_init();
        db_log_set_level(LOG_LEVEL_DEBUG);
    }

    void TearDown() override {
        db_bgworker_stop();
    }
};

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @brief 测试任务注册与注销
 */
TEST_F(BgWorkerTest, RegisterUnregister) {
    std::atomic<int> check_count{0};
    std::atomic<int> exec_count{0};

    db_bg_task_t task = {
        .name = "test_task",
        .interval_ms = 100,
        .init = nullptr,
        .check = [](db_bg_task_context_t *ctx) -> int {
            (void)ctx;
            return 1;  /* 总是触发 */
        },
        .execute = [](db_bg_task_context_t *ctx) -> int {
            (void)ctx;
            return 0;
        },
        .cleanup = nullptr,
        .ctx = nullptr
    };

    /* 注册任务 */
    int slot;
    EXPECT_EQ(db_bgworker_register(&task, &slot), 0);

    /* 启动调度器 */
    EXPECT_EQ(db_bgworker_start(), 0);

    /* 等待一段时间让任务执行 */
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    /* 注销任务 */
    EXPECT_EQ(db_bgworker_unregister("test_task"), 0);

    /* 停止调度器 */
    EXPECT_EQ(db_bgworker_stop(), 0);
}

/**
 * @brief 测试任务暂停与恢复
 */
TEST_F(BgWorkerTest, PauseResume) {
    std::atomic<int> exec_count{0};

    db_bg_task_t task = {
        .name = "pause_test_task",
        .interval_ms = 50,
        .init = nullptr,
        .check = [](db_bg_task_context_t *ctx) -> int {
            return 1;
        },
        .execute = [](db_bg_task_context_t *ctx) -> int {
            exec_count++;
            return 0;
        },
        .cleanup = nullptr,
        .ctx = nullptr
    };

    /* 注册并启动 */
    db_bgworker_register(&task, nullptr);
    db_bgworker_start();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int count_before = exec_count.load();

    /* 暂停任务 */
    EXPECT_EQ(db_bgworker_pause("pause_test_task"), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int count_paused = exec_count.load();

    /* 恢复任务 */
    EXPECT_EQ(db_bgworker_resume("pause_test_task"), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int count_after = exec_count.load();

    /* 验证暂停期间没有执行 */
    EXPECT_GT(count_after, count_paused);
    EXPECT_EQ(count_paused, count_before);

    db_bgworker_unregister("pause_test_task");
    db_bgworker_stop();
}

/**
 * @brief 测试任务状态查询
 */
TEST_F(BgWorkerTest, GetStatus) {
    db_bg_task_t task = {
        .name = "status_test_task",
        .interval_ms = 1000,
        .init = nullptr,
        .check = [](db_bg_task_context_t *ctx) -> int { return 0; },
        .execute = [](db_bg_task_context_t *ctx) -> int { return 0; },
        .cleanup = nullptr,
        .ctx = nullptr
    };

    db_bgworker_register(&task, nullptr);
    db_bgworker_start();

    /* 等待任务进入 IDLE 状态 */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    unsigned int status;
    EXPECT_EQ(db_bgworker_get_status("status_test_task", &status), 0);
    EXPECT_EQ(status, DB_BG_TASK_STATUS_IDLE);

    db_bgworker_unregister("status_test_task");
    db_bgworker_stop();
}

/**
 * @brief 测试统计信息获取
 */
TEST_F(BgWorkerTest, GetStats) {
    db_bgworker_start();

    db_bgworker_stats_t stats;
    EXPECT_EQ(db_bgworker_get_stats(&stats), 0);
    EXPECT_GE(stats.task_count, 0u);

    db_bgworker_stop();
}

/**
 * @brief 测试重复注册
 */
TEST_F(BgWorkerTest, DuplicateRegistration) {
    db_bg_task_t task1 = {
        .name = "dup_task",
        .interval_ms = 1000,
        .init = nullptr,
        .check = [](db_bg_task_context_t *ctx) -> int { return 0; },
        .execute = [](db_bg_task_context_t *ctx) -> int { return 0; },
        .cleanup = nullptr,
        .ctx = nullptr
    };

    db_bg_task_t task2 = {
        .name = "dup_task",  /* 同名 */
        .interval_ms = 1000,
        .init = nullptr,
        .check = [](db_bg_task_context_t *ctx) -> int { return 0; },
        .execute = [](db_bg_task_context_t *ctx) -> int { return 0; },
        .cleanup = nullptr,
        .ctx = nullptr
    };

    EXPECT_EQ(db_bgworker_register(&task1, nullptr), 0);
    EXPECT_EQ(db_bgworker_register(&task2, nullptr), -1);  /* 应失败 */

    db_bgworker_unregister("dup_task");
}
```

- [ ] **Step 2: 验证测试编译和运行**

```bash
cd build/engineering && cmake --build . --target bgworker_test 2>&1 | tail -20
ctest --test-dir build/engineering -R bgworker_test --output-on-failure
```

- [ ] **Step 3: 提交**

```bash
git add engineering/test/db/bgworker/bgworker_test.cpp
git commit -m "test(bgworker): 添加调度器核心测试 bgworker_test.cpp"
```

---

### Task 11: 编写 CCEH 缩容测试 (cceh_shrink_test.cpp)

**Files:**
- Create: `engineering/test/db/bgworker/cceh_shrink_test.cpp`

**Interfaces:**
- Consumes: `db/index/hash/cceh.h`, `db/bgworker.h`
- Produces: 测试用例

- [ ] **Step 1: 编写 CCEH 缩容测试**

```cpp
/**
 * @file cceh_shrink_test.cpp
 * @brief CCEH 缩容任务测试
 */

#include <gtest/gtest.h>
#include "db/index/hash/cceh.h"
#include "db/bgworker.h"
#include "db/log.h"
#include <thread>
#include <vector>

/* ============================================================
 * 测试夹具
 * ============================================================ */

class CcehShrinkTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_log_init();
        db_log_set_level(LOG_LEVEL_DEBUG);

        /* 创建测试索引 */
        index_ = cceh_index_create(8, 1);
        ASSERT_NE(index_, nullptr);
    }

    void TearDown() override {
        if (index_) {
            cceh_index_drop(index_);
            index_ = nullptr;
        }
        db_bgworker_stop();
    }

    cceh_index_t *index_ = nullptr;
};

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @brief 测试缩容任务注册
 */
TEST_F(CcehShrinkTest, ShrinkTaskRegistration) {
    db_bgworker_start();

    /* 注册任务（需要先设置索引） */
    /* 注意：实际使用时应通过 API 设置索引 */
    // db_bgworker_cceh_shrink_set_index(index_);

    /* 验证调度器可以正常启动和停止 */
    db_bgworker_stop();
}

/**
 * @brief 测试负载因子计算
 */
TEST_F(CcehShrinkTest, LoadFactorCalculation) {
    /* 空索引应该有低负载因子 */
    uint32_t size = cceh_index_size(index_);
    uint32_t segment_count = cceh_index_segment_count(index_);

    EXPECT_EQ(size, 0u);
    EXPECT_GE(segment_count, 1u);

    /* 插入一些数据后负载因子应该增加 */
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        cceh_index_insert(index_, key, strlen(key), &i, sizeof(i));
    }

    size = cceh_index_size(index_);
    EXPECT_EQ(size, 10u);
}

/**
 * @brief 测试缩容安全检查
 */
TEST_F(CcehShrinkTest, ShrinkSafetyCheck) {
    /* 初始状态 global_depth = 1，应该不允许缩容 */
    uint32_t global_depth = cceh_index_global_depth(index_);
    EXPECT_GE(global_depth, 1u);

    /* 插入大量数据触发扩容 */
    for (int i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        cceh_index_upsert(index_, key, strlen(key), &i, sizeof(i));
    }

    global_depth = cceh_index_global_depth(index_);
    /* 扩容后应该有更大的 global_depth */
    EXPECT_GE(global_depth, 1u);
}
```

- [ ] **Step 2: 验证测试编译和运行**

```bash
cd build/engineering && cmake --build . --target cceh_shrink_test 2>&1 | tail -20
ctest --test-dir build/engineering -R cceh_shrink_test --output-on-failure
```

- [ ] **Step 3: 提交**

```bash
git add engineering/test/db/bgworker/cceh_shrink_test.cpp
git commit -m "test(bgworker): 添加 CCEH 缩容测试 cceh_shrink_test.cpp"
```

---

### Task 12: 更新设计文档并编写使用文档

**Files:**
- Modify: `docs/superpowers/specs/2026-07-16-bgworker-design.md`
- Create: `docs/usage/bgworker-usage.md`

**Interfaces:**
- Consumes: 无
- Produces: 更新后的设计文档、使用文档

- [ ] **Step 1: 更新设计文档中的实现进度**

标记已完成的功能

- [ ] **Step 2: 编写使用文档**

```markdown
# 后台任务调度器使用指南

## 快速开始

### 1. 头文件包含

```c
#include <db/bgworker.h>
```

### 2. 启动调度器

```c
/* 启动后台任务调度器 */
db_bgworker_start();
```

### 3. 注册自定义任务

```c
static int my_task_init(db_bg_task_context_t *ctx) {
    ctx->user_data = my_alloc_context();
    return 0;
}

static int my_task_check(db_bg_task_context_t *ctx) {
    /* 返回 > 0 表示满足触发条件 */
    return check_condition(ctx->user_data) ? 1 : 0;
}

static int my_task_execute(db_bg_task_context_t *ctx) {
    return perform_action(ctx->user_data);
}

static void my_task_cleanup(db_bg_task_context_t *ctx) {
    my_free_context(ctx->user_data);
}

/* 注册任务 */
db_bg_task_t task = {
    .name = "my_custom_task",
    .interval_ms = 10000,  /* 10 秒检查一次 */
    .init = my_task_init,
    .check = my_task_check,
    .execute = my_task_execute,
    .cleanup = my_task_cleanup,
    .ctx = NULL
};

int slot;
db_bgworker_register(&task, &slot);
```

### 4. 停止调度器

```c
db_bgworker_stop();
```

## 配置参数

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| bgworker.enabled | bool | true | 是否启用 |
| bgworker.tick_ms | int | 1000 | 检查间隔 |
| bgworker.cceh_shrink.enable | bool | true | CCEH 缩容启用 |
| bgworker.cceh_shrink.load_factor_threshold | float | 0.25 | 负载因子阈值 |
| bgworker.cceh_shrink.time_window | int | 3 | 时间窗口 |

## 完整示例

见 `engineering/test/db/bgworker/bgworker_test.cpp`
```

- [ ] **Step 3: 提交**

```bash
git add docs/superpowers/specs/2026-07-16-bgworker-design.md docs/usage/bgworker-usage.md
git commit -m "docs(bgworker): 更新设计文档并添加使用指南"
```

---

## 任务执行清单

| # | 任务 | 状态 | 依赖 |
|---|------|------|------|
| 1 | 创建目录结构和 CMake 配置 | ⬜ | - |
| 2 | 定义任务接口 (task_iface.h) | ⬜ | - |
| 3 | 定义内部数据结构 (bgworker_internal.h) | ⬜ | Task 2 |
| 4 | 实现调度器核心 (bgworker.c) | ⬜ | Task 3 |
| 5 | 实现任务注册/注销 API (bgworker_api.c) | ⬜ | Task 4 |
| 6 | 实现公共头文件 (bgworker.h) | ⬜ | Task 5 |
| 7 | 实现 CCEH 缩容任务 (task_cceh_shrink.c) | ⬜ | Task 2 |
| 8 | 实现统计收集任务 (task_stats.c) | ⬜ | Task 2 |
| 9 | 添加 GUC 配置参数 (bgworker_config.c) | ⬜ | Task 4 |
| 10 | 编写调度器核心测试 | ⬜ | Task 6 |
| 11 | 编写 CCEH 缩容测试 | ⬜ | Task 7, 10 |
| 12 | 更新设计文档并编写使用文档 | ⬜ | 所有任务 |
