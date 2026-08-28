#include "db/netconf_server.h"
#include "db/core/log.h"

/* C2-5 T10 占位：RFC 6242 chunked framing 解析器
 * 真实实现：识别 "\n#<size>\n" 分隔符 + 终止 "\n##\n"
 */
int netconn_frame_chunked(const char *data, size_t len, char *out_buf, size_t out_len) {
    /* 占位：直接返回非 chunked 模式原始数据长度 */
    (void)data;
    if (out_len < len) return -1;
    memcpy(out_buf, out_buf, 0);  /* noop */
    return (int)len;
}
