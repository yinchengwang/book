# mmsdk-go — P1 多模态嵌入式 SDK Go 绑定

通过 cgo 调用 C ABI，提供 Go 语言对向量 / 图 / 时序 / 文本四种模型的操作接口。

## 平台要求

- Go 1.19+（cgo 支持）
- 已构建的 `libmmsdk.a`（C 静态库）
- 已构建的 `libsqlite3.a`（SQLite 静态库）
- C 编译器（推荐 GCC/Clang/MSVC）

## 构建 C 静态库

```bash
cmake -S . -B build/engineering -G Ninja -DCMAKE_BUILD_TYPE=Release -DENGINEERING_BUILD=ON
cmake --build build/engineering --target mmsdk
```

构建产物路径：
- `build/engineering/engineering/src/sdk/libmmsdk.a`
- `build/engineering/sdk_sqlite3_build/libsqlite3.a`

## 安装 Go SDK

```bash
cd engineering/sdk/go/multimodal
go mod init github.com/multimodal/multimodal  # 首次
go mod tidy
```

## 运行测试

集成测试需要动态链接 C 库，使用 build tag 控制：

```bash
# 纯编译检查（不需要 C 库）
go vet ./...
go build ./...

# 集成测试（需要 CGO 配置）
export CGO_CFLAGS="-I$(pwd)/../../include -I$(pwd)/../../third_part/sqlite3"
export CGO_LDFLAGS="-L$(pwd)/../../build/engineering/engineering/src/sdk -L$(pwd)/../../build/engineering/sdk_sqlite3_build -lmmsdk -lsqlite3"
go test -tags=integration -v ./...
```

## 使用示例

```go
package main

import (
    "fmt"
    "log"

    "github.com/multimodal/multimodal"
)

func main() {
    db, err := multimodal.Open("my_data.db")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    // 创建向量集合
    if err := db.CreateCollection("embeddings", multimodal.ModelVector, 128); err != nil {
        log.Fatal(err)
    }

    // 批量添加向量
    if err := db.VectorAdd("embeddings",
        []string{"doc1", "doc2"},
        [][]float32{{0.1, 0.2, 0.3}, {0.4, 0.5, 0.6}}); err != nil {
        log.Fatal(err)
    }

    // 向量搜索
    hits, err := db.VectorSearch("embeddings", []float32{0.1, 0.2, 0.3}, 5)
    if err != nil {
        log.Fatal(err)
    }
    for _, hit := range hits {
        fmt.Printf("id=%s distance=%.4f\n", hit.ID, hit.Distance)
    }
}
```

## API 概览

### 生命周期
- `Open(path string) (*DB, error)` — 打开或创建数据库
- `(*DB).Close() error` — 关闭数据库
- `(*DB).LastError() (int, string)` — 获取最近一次错误码和消息

### 集合管理
- `(*DB).CreateCollection(name string, model Model, vectorDim uint32) error`
- `(*DB).DropCollection(name string) error`

### 数据模型
| 方法 | 说明 |
|------|------|
| `VectorAdd(coll, ids, embeddings) error` | 批量添加向量 |
| `VectorSearch(coll, query, topK) ([]Hit, error)` | 向量 KNN 搜索 |
| `GraphAddNode(coll, id, label, props) error` | 添加图节点 |
| `GraphAddEdge(coll, src, dst, label, props) error` | 添加图边 |
| `TSAppend(coll, ts, value, tags) error` | 追加时序数据点 |
| `TSQuery(coll, start, end) ([]map, error)` | 时序范围查询 |
| `TextAdd(coll, id, text, meta) error` | 添加文本条目 |
| `TextSearch(coll, query, topK) ([]Hit, error)` | 全文搜索 |

## 已知限制

1. **时序查询字段映射简化**：`TSQuery` 返回的 `timestamp` 实际通过 `distance` 字段映射（精度受限）
2. **图遍历 / 全文排序**：高级查询（path 查询、FTS5 排序）暂未暴露
3. **Windows cgo**：需配置 MinGW 或 MSVC，详见上面 CGO_LDFLAGS 说明
4. **错误恢复**：当前未实现 panic → C 错误转换，cgo 调用 panic 会导致进程崩溃

## 跨平台构建

### Linux/macOS
```bash
CGO_ENABLED=1 go build ./...
```

### Windows (MinGW)
```bash
CGO_ENABLED=1 CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ go build ./...
```

### Windows (MSVC)
需安装 Visual Studio Build Tools，并设置：
```bash
set CGO_ENABLED=1
go build ./...
```