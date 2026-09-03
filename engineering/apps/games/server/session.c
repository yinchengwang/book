/*
 * 会话管理设计（待实现）
 *
 * 数据结构：
 *   - 单进程内存哈希表：session_id → 游戏实例指针
 *   - 游戏实例：G2048Game* 或 SnakeGame*（union 或 void*）
 *   - 使用 uthash（third_part/uthash/）管理哈希表
 *
 * REST 接口：
 *   POST /game/new?type=snake|2048
 *     → 创建游戏实例，分配 session_id，返回 JSON { "id": "xxx" }
 *   POST /game/{id}/input  body: { "dir": 0-3 }
 *     → 调用 snake_input / g2048_move，返回 { "ok": true }
 *   GET /game/{id}/state
 *     → 调用 g2048_snapshot / snake_get_state，返回 JSON 棋盘状态
 *
 * 会话表操作（TODO）：
 *   - session_create(type) → id
 *   - session_get(id) → 游戏指针或 NULL
 *   - session_remove(id)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TODO: 包含游戏核心头文件 */
#include "../core/g2048_core.h"
#include "../core/snake_core.h"

typedef enum { GAME_2048, GAME_SNAKE } GameType;

typedef struct Session {
    char id[32];
    GameType type;
    void *game;  /* G2048Game* 或 SnakeGame* */
    struct Session *next;
} Session;

/* TODO: 实现会话表操作函数 */
