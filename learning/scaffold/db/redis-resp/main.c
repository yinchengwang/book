/**
 * @file main.c
 * @brief Redis RESP 协议演示
 *
 * 演示 Redis Serialization Protocol (RESP) 的类型编码、解析器状态机、
 * bulk string、array、以及管道化命令。
 *
 * @see engineering/src/redis/resp.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

/* ============================================================
 * RESP 协议类型
 * ============================================================ */

/** RESP 类型前缀 */
typedef enum {
    RESP_STRING   = '+',   /* 简单字符串 */
    RESP_ERROR    = '-',   /* 错误 */
    RESP_INTEGER  = ':',   /* 整数 */
    RESP_BULK     = '$',   /* Bulk String */
    RESP_ARRAY    = '*',   /* Array */
    RESP_UNKNOWN  = 0
} RespType;

/** 解析器状态 */
typedef enum {
    PARSE_START,
    PARSE_TYPE,
    PARSE_LENGTH,
    PARSE_DATA,
    PARSE_CRLF1,
    PARSE_CRLF2,
    PARSE_DONE
} ParserState;

/** RESP 值（前置声明）*/
typedef struct RespValue_s RespValue;

/** RESP 值 */
struct RespValue_s {
    RespType       type;
    int64_t        integer;
    char          *str;
    size_t         len;
    int64_t        length;    /* Bulk String 长度 */
    struct {
        RespValue **elements;  /* 数组元素指针 */
        size_t     count;
    } array;
};

/** 解析器上下文 */
typedef struct {
    ParserState    state;
    RespType       type;
    int64_t        length;
    int64_t        elements;    /* 数组元素计数 */
    RespValue     *root;        /* 根值 */
    RespValue     *current;     /* 当前解析的值 */
    size_t         buf_pos;
    char           buffer[4096];
} RespParser;

/* ============================================================
 * [resp] RESP 类型编码
 * ============================================================ */

static void print_resp_type(RespType type)
{
    switch (type) {
    case RESP_STRING:  printf("+ (简单字符串)"); break;
    case RESP_ERROR:   printf("- (错误)");      break;
    case RESP_INTEGER: printf(": (整数)");       break;
    case RESP_BULK:    printf("$ (Bulk String)"); break;
    case RESP_ARRAY:   printf("* (数组)");       break;
    default:           printf("? (未知)");        break;
    }
}

static void demo_resp_types(void)
{
    printf("[resp] ===== RESP 协议类型 =====\n\n");

    /* 简单字符串 */
    printf("[resp] 简单字符串 (+):\n");
    printf("[resp]   +OK\\r\\n\n");
    printf("[resp]   用途: 成功响应，如 +OK, +PONG\n\n");

    /* 错误 */
    printf("[resp] 错误 (-):\n");
    printf("[resp]   -ERR unknown command\\r\\n\n");
    printf("[resp]   用途: 错误信息，如 -WRONGTYPE, -ERR\n\n");

    /* 整数 */
    printf("[resp] 整数 (:):\n");
    printf("[resp]   :1000\\r\\n\n");
    printf("[resp]   用途: INCR/DBSIZE/LLEN 等返回值\n\n");

    /* Bulk String */
    printf("[resp] Bulk String ($):\n");
    printf("[resp]   $5\\r\\nhello\\r\\n\n");
    printf("[resp]   $11\\r\\nhello world\\r\\n\n");
    printf("[resp]   $-1\\r\\n  (NULL)\n\n");
    printf("[resp]   用途: 二进制安全字符串\n\n");

    /* Array */
    printf("[resp] 数组 (*):\n");
    printf("[resp]   *2\\r\\n+PONG\\r\\n:1000\\r\\n\n");
    printf("[resp]   用途: 命令和批量响应的容器\n\n");
}

/* ============================================================
 * [cmd] 命令编码
 * ============================================================ */

static void encode_command(char *buf, size_t buf_size, const char *cmd, int argc, char **argv)
{
    size_t pos = 0;

    /* 数组长度 */
    pos += snprintf(buf + pos, buf_size - pos, "*%d\r\n", argc);

    for (int i = 0; i < argc && pos < buf_size - 1; i++) {
        size_t len = strlen(argv[i]);
        pos += snprintf(buf + pos, buf_size - pos, "$%zu\r\n", len);
        memcpy(buf + pos, argv[i], len);
        pos += len;
        pos += snprintf(buf + pos, buf_size - pos, "\r\n");
    }
}

static void demo_command_encoding(void)
{
    printf("[cmd] ===== 命令编码 =====\n\n");

    char buf[4096];
    char *argv[4];

    /* PING 命令 */
    argv[0] = "PING";
    encode_command(buf, sizeof(buf), "PING", 1, argv);
    printf("[cmd] PING -> %s", buf);

    /* GET key */
    argv[0] = "GET";
    argv[1] = "mykey";
    encode_command(buf, sizeof(buf), "GET", 2, argv);
    printf("[cmd] GET mykey -> %s", buf);

    /* SET key value */
    argv[0] = "SET";
    argv[1] = "name";
    argv[2] = "redis";
    encode_command(buf, sizeof(buf), "SET", 3, argv);
    printf("[cmd] SET name redis -> %s", buf);

    /* MSET */
    argv[0] = "MSET";
    argv[1] = "k1";
    argv[2] = "v1";
    argv[3] = "k2";
    encode_command(buf, sizeof(buf), "MSET", 4, argv);
    printf("[cmd] MSET k1 v1 k2 -> %s", buf);

    printf("\n");
}

/* ============================================================
 * [parse] RESP 解析器（简化版）
 * ============================================================ */

static RespValue *resp_create_value(RespType type)
{
    RespValue *v = calloc(1, sizeof(RespValue));
    v->type = type;
    return v;
}

static void resp_free_value(RespValue *v)
{
    if (!v) return;
    if (v->str) free(v->str);
    if (v->array.elements) {
        for (size_t i = 0; i < v->array.count; i++) {
            resp_free_value(v->array.elements[i]);
        }
        free(v->array.elements);
    }
    free(v);
}

/** 解析一行 CRLF 结尾的数据 */
static int parse_line(char *data, size_t len, int64_t *out_value)
{
    if (len < 2 || data[len-2] != '\r' || data[len-1] != '\n') {
        return -1;
    }
    char *end;
    *out_value = strtoll(data, &end, 10);
    return 0;
}

/** 模拟解析简单字符串 */
static RespValue *parse_simple_string(const char *data)
{
    RespValue *v = resp_create_value(RESP_STRING);
    size_t len = strlen(data);
    /* 去除 \r\n */
    v->str = malloc(len);
    memcpy(v->str, data, len);
    if (len >= 2 && v->str[len-1] == '\n' && v->str[len-2] == '\r') {
        v->len = len - 2;
        v->str[v->len] = '\0';
    }
    return v;
}

/** 模拟解析整数 */
static RespValue *parse_integer(const char *data)
{
    RespValue *v = resp_create_value(RESP_INTEGER);
    char *end;
    v->integer = strtoll(data, &end, 10);
    return v;
}

/** 模拟解析 Bulk String */
static RespValue *parse_bulk_string(const char *data)
{
    RespValue *v = resp_create_value(RESP_BULK);

    /* 解析长度 */
    char line[64];
    size_t i = 0;
    while (i < 63 && data[i] && data[i] != '\r') {
        line[i] = data[i];
        i++;
    }
    line[i] = '\0';

    int64_t len;
    if (parse_line(line, strlen(line), &len) != 0) {
        return v;
    }
    v->length = len;

    /* -1 表示 NULL */
    if (len == -1) {
        v->str = NULL;
        return v;
    }

    /* 跳过 \r\n */
    const char *content = data + i + 2;
    v->str = malloc(len + 1);
    memcpy(v->str, content, len);
    v->str[len] = '\0';
    v->len = len;
    return v;
}

/** 模拟解析 Array */
static RespValue *parse_array(const char *data)
{
    RespValue *v = resp_create_value(RESP_ARRAY);

    /* 解析元素数量 */
    char line[64];
    size_t i = 0;
    while (i < 63 && data[i] && data[i] != '\r') {
        line[i] = data[i];
        i++;
    }
    line[i] = '\0';

    int64_t count;
    if (parse_line(line, strlen(line), &count) != 0) {
        return v;
    }
    v->array.count = count;
    v->array.elements = calloc(count, sizeof(RespValue *));

    return v;
}

static void demo_resp_parser(void)
{
    printf("[parse] ===== RESP 解析器 =====\n\n");

    /* 解析简单字符串 */
    printf("[parse] 解析简单字符串:\n");
    const char *simple = "+OK\r\n";
    RespValue *v = parse_simple_string(simple);
    printf("[parse]   类型: ");
    print_resp_type(v->type);
    printf("\n[parse]   值: %s\n", v->str);
    resp_free_value(v);

    /* 解析整数 */
    printf("\n[parse] 解析整数:\n");
    const char *integer = ":1000\r\n";
    v = parse_integer(integer);
    printf("[parse]   类型: ");
    print_resp_type(v->type);
    printf("\n[parse]   值: %lld\n", (long long)v->integer);
    resp_free_value(v);

    /* 解析 Bulk String */
    printf("\n[parse] 解析 Bulk String:\n");
    const char *bulk = "$5\r\nhello\r\n";
    v = parse_bulk_string(bulk);
    printf("[parse]   类型: ");
    print_resp_type(v->type);
    printf("\n[parse]   长度: %lld\n", (long long)v->length);
    printf("[parse]   值: %s\n", v->str);
    resp_free_value(v);

    /* 解析 NULL Bulk String */
    printf("\n[parse] 解析 NULL Bulk:\n");
    const char *null_bulk = "$-1\r\n";
    v = parse_bulk_string(null_bulk);
    printf("[parse]   类型: ");
    print_resp_type(v->type);
    printf("\n[parse]   长度: %lld (NULL 表示)\n", (long long)v->length);
    resp_free_value(v);

    printf("\n");
}

/* ============================================================
 * [pipe] 管道化
 * ============================================================ */

static void demo_pipelining(void)
{
    printf("[pipe] ===== 管道化（Pipelining）=====\n\n");

    printf("[pipe] 无管道:\n");
    printf("[pipe]   Client -> PING\n");
    printf("[pipe]   Server -> +PONG\n");
    printf("[pipe]   Client -> WAIT 1 1000\n");
    printf("[pipe]   Server -> :1\n");
    printf("[pipe]   往返延迟累加 2x RTT\n\n");

    printf("[pipe] 管道化:\n");
    printf("[pipe]   Client -> *1\\r\\n+PING\\r\\n");
    printf("[pipe]           *3\\r\\n+SET\\r\\n$3\\r\\nkey\\r\\n$5\\r\\nvalue\\r\\n");
    printf("[pipe]           *2\\r\\n+GET\\r\\n$3\\r\\nkey\\r\\n\n");
    printf("[pipe]   Server -> +PONG\\r\\n+OK\\r\\n$5\\r\\nvalue\\r\\n\n");
    printf("[pipe]   优点: N 个命令只需 1 次 RTT\n\n");

    /* 模拟管道 */
    printf("[pipe] 管道示例:\n");
    char buf[4096];
    size_t pos = 0;

    /* 命令 1: PING */
    memcpy(buf + pos, "*1\r\n+PING\r\n", 10);
    pos += 10;

    /* 命令 2: INCR counter */
    memcpy(buf + pos, "*2\r\n+INCR\r\n+counter\r\n", 22);
    pos += 22;

    /* 命令 3: GET counter */
    memcpy(buf + pos, "*2\r\n+GET\r\n+counter\r\n", 21);
    pos += 21;

    printf("[pipe]   发送 %zu 字节管道请求:\n", pos);
    for (size_t i = 0; i < pos; i++) {
        char c = buf[i];
        if (c == '\r') printf("\\r");
        else if (c == '\n') printf("\\n\n[pipe]   ");
        else printf("%c", c);
    }
    printf("\n\n");
}

/* ============================================================
 * [proto] 协议细节
 * ============================================================ */

static void demo_protocol_details(void)
{
    printf("[proto] ===== RESP 协议细节 =====\n\n");

    /* 二进制安全 */
    printf("[proto] 二进制安全:\n");
    printf("[proto]   $11\\r\\nhello world\\r\\n\n");
    printf("[proto]   长度前缀确保安全处理 \\x00 \\r \\n\n\n");

    /* 嵌套数组 */
    printf("[proto] 嵌套数组（LRANGE）:\n");
    printf("[proto]   *2\\r\\n");
    printf("[proto]     *3\\r\\n");
    printf("[proto]       $5\\r\\nitem1\\r\\n");
    printf("[proto]       $5\\r\\nitem2\\r\\n");
    printf("[proto]       $5\\r\\nitem3\\r\\n");
    printf("[proto]     *2\\r\\n");
    printf("[proto]       $5\\r\\nitem4\\r\\n");
    printf("[proto]       $5\\r\\nitem5\\r\\n\n");

    /* 常用命令响应 */
    printf("[proto] 常用命令响应:\n");
    printf("[proto]   GET    -> $len\\r\\nvalue\\r\\n 或 $-1\\r\\n\n");
    printf("[proto]   SET    -> +OK\\r\\n\n");
    printf("[proto]   INCR   -> :new_value\\r\\n\n");
    printf("[proto]   LLEN   -> :length\\r\\n\n");
    printf("[proto]   HGETALL -> *n\\r\\n...\\r\\n\n");
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void)
{
    printf("========================================\n");
    printf("   Redis RESP 协议演示\n");
    printf("========================================\n\n");

    demo_resp_types();
    demo_command_encoding();
    demo_resp_parser();
    demo_pipelining();
    demo_protocol_details();

    printf("========================================\n");
    printf("   演示结束\n");
    printf("========================================\n");
    return 0;
}
