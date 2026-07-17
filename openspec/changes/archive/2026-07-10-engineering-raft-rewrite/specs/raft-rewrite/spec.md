# S26 Spec —— Engineering Raft Smoke Test

## 1. 测试契约

新增 `raft_smoke_test.cpp` 包含：

```cpp
TEST(RaftSmokeTest, ServerCreate) {
    RaftServer *server = raft_server_create(1, "127.0.0.1:8080");
    EXPECT_NE(server, nullptr);
    raft_server_destroy(server);
}

TEST(RaftSmokeTest, LeaderQuery) {
    RaftServer *server = raft_server_create(1, nullptr);
    EXPECT_EQ(raft_get_leader(server), UINT64_MAX);  // 无 leader
    raft_server_destroy(server);
}
```

## 2. 不做

- ❌ 重写 raft.c 业务
- ❌ 学习层副本（phase10+ 重做）
- ❌ 测试 raft 真正选举（业务逻辑已占位）
