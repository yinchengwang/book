# P1 多模态嵌入式 SDK 集成测试与性能基准

本目录包含三层验证：

| 层级 | 工具 | 文件 | 验证范围 |
|------|------|------|----------|
| C++ 单元 | GoogleTest | `engineering/test/sdk/integration/cross_lang_consistency_test.cpp` | C ABI 一致性、性能基准、并发安全、错误恢复 |
| Python | pytest | `engineering/sdk/python/tests/test_basic.py` | pybind11 绑定、上下文管理器 |
| Go | go test | `engineering/sdk/go/multimodal/integration_test.go` | cgo 绑定、跨平台编译 |

## 运行方式

### C++ 集成测试（推荐）

```bash
cmake --build build/engineering --target cross_lang_consistency_test
cd build/engineering/sdk_integration_tests
./cross_lang_consistency_test.exe
```

### Python 集成测试

```bash
cd engineering/sdk/python
/c/Users/yinch/anaconda3/python.exe -m pytest tests/test_basic.py -v
```

### Go 集成测试

```bash
export PATH="/c/Program Files/Go/bin:$PATH"
export CGO_CFLAGS="-ID:/code/book/engineering/include -ID:/code/book/third_part/sqlite3"
export CGO_LDFLAGS="-LD:/code/book/build/engineering/engineering/src/sdk -LD:/code/book/build/engineering/sdk_sqlite3_build -lmmsdk -lsqlite3"
cd engineering/sdk/go/multimodal
go test -tags=integration -v .
```

## 性能基准（实测数据）

测试环境：Windows 11 + MinGW GCC 14.2.0 + Release 模式

### 向量 KNN

| 数据规模 | 插入速度 | 搜索 QPS | 备注 |
|----------|---------|---------|------|
| 1000×128, 100 次查询 | ~8400 vec/s | ~1858 qps | K=10，4 维单位向量 + 随机干扰 |

### 时序

| 数据规模 | 追加速度 | 范围查询 |
|----------|---------|---------|
| 10000 个点 | ~16906 pt/s | 2.4ms（count=10000） |

### 文件大小

| 数据规模 | 文件大小 |
|----------|---------|
| 1000×128 向量 | 612 KiB |

### 并发

4 线程 × 50 次并发读：0 错误

## 跨语言一致性

三种语言（C++ / Python / Go）共享同一 SQLite 文件，互读写数据无障碍：

- C ABI 是 FFI 锚点（所有语言最终都走 C）
- Python 通过 pybind11 调用 C ABI
- Go 通过 cgo 调用 C ABI
- C++ RAII 在 C ABI 之上加封装

详见 `cross_lang_e2e.sh`（Linux/macOS 适用）。