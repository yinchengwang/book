# NOTES - 工程对照

## 工程源码对照

`engineering/src/db/api/rest_api.c` 中涉及的网络连接问题排查：

### 1. TIME_WAIT 问题

我们的 REST API 服务器在连接关闭时会产生 TIME_WAIT：

```c
// rest_api.c:466, 482, 503, 557
closesocket(client);  // 关闭客户端连接
```

如果频繁创建/关闭连接，会累积大量 TIME_WAIT 状态的 socket。

**工程解决方案：**
- 使用 `SO_REUSEADDR` 选项（在 socket 创建后、bind 前设置）
- 服务器端连接使用长连接（Connection: keep-alive）
- 调高本地端口范围

### 2. CLOSE_WAIT 问题

代码中正确处理了连接关闭：

```c
// rest_api.c:465-467
if (bytes_received <= 0) {
    closesocket(client);  // 读取失败或对端关闭，关闭本端
    continue;
}
```

这是正确的处理方式。如果漏掉这个分支，会导致 CLOSE_WAIT 累积。

### 3. 连接超时

```c
// rest_api.c:573
.connection_timeout = 30,
```

我们设置了连接超时，这是防止 CLOSE_WAIT 的重要手段。

### 4. 连接状态监控

生产环境应定期检查连接状态：

```bash
# 检查 DB 服务器的连接
ss -ant | grep :5432  # PostgreSQL 端口
ss -ant | grep :8888  # 我们的 REST API

# 检查是否有异常状态的连接
ss -ant | awk '{print $1}' | sort | uniq -c
```

## DB 网络问题排查清单

| 症状 | 可能原因 | 排查命令 |
|------|---------|---------|
| 连接被拒绝 | 服务未启动/端口错 | `ss -tlnp \| grep port` |
| 连接超时 | 防火墙/网络问题 | `ping` + `telnet` |
| CLOSE_WAIT 累积 | 代码未调用 close() | `ss -ant \| grep CLOSE-WAIT` |
| TIME_WAIT 累积 | 频繁短连接 | `ss -ant \| grep TIME-WAIT` |
| 连接数暴涨 | 资源泄漏/攻击 | `ss -s` + `ss -ant` |

## 工程实践

1. **始终设置超时**：防止客户端不发送数据也不关闭连接
2. **正确处理 close()**：读取返回 0 或负值时必须关闭 socket
3. **使用 SO_REUSEADDR**：允许重启服务时绑定同一端口
4. **监控连接状态**：定期检查 TIME_WAIT/CLOSE_WAIT 数量

## 学习路径

网络诊断是排查"连接突然断掉"问题的基础。配合：
- **socket** 卡 → 理解连接建立过程
- **epoll** 卡 → 理解事件驱动下的连接管理
- **本卡** → 掌握问题排查技能
