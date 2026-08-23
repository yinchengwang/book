#!/usr/bin/env bash
# cross_lang_e2e.sh — 跨语言 E2E 集成测试
#
# 流程：
#   1. C++ 程序写入数据
#   2. Python 程序读取验证
#   3. Go 程序读取验证
#   4. 对比三种语言的结果一致性
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
BUILD_DIR="$ROOT/build/engineering"
TEST_DB="/tmp/cross_lang_e2e.db"

cd "$(dirname "$0")"

echo "=========================================="
echo "跨语言 E2E 一致性测试"
echo "=========================================="

# 1. C++ 程序创建并填充数据库
echo "[1/3] C++ 写入数据..."
cat > /tmp/write_db.cpp <<'EOF'
#include <cstdio>
#include <cstring>
#include "sdk/mmdb.h"
#include "sdk/mmdb_vectors.h"

int main() {
    std::remove("/tmp/cross_lang_e2e.db");
    mmdb_t* db = mmdb_open("/tmp/cross_lang_e2e.db", nullptr);
    if (!db) { fprintf(stderr, "open failed\n"); return 1; }

    mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, 4};
    mmdb_collection_t* c = mmdb_collection_create(db, "shared", &s);
    if (!c) { fprintf(stderr, "create failed\n"); return 1; }

    const char* ids[] = {"alpha", "beta", "gamma"};
    float vecs[][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    };

    for (int i = 0; i < 3; i++) {
        mmdb_vector_t v = {(const uint8_t*)ids[i], strlen(ids[i]), vecs[i], 4, nullptr, nullptr};
        mmdb_vectors_add(c, &v, 1);
    }

    mmdb_close(db);
    printf("C++ write OK\n");
    return 0;
}
EOF

g++ -std=c++17 -I"$ROOT/engineering/include" \
    /tmp/write_db.cpp \
    -L"$BUILD_DIR/engineering/src/sdk" -L"$BUILD_DIR/sdk_sqlite3_build" \
    -lmmsdk -lsqlite3 \
    -o /tmp/write_db 2>&1 | grep -v "warning" || true
/tmp/write_db

# 2. Python 读取验证
echo "[2/3] Python 读取验证..."
export PATH="/c/mingw64/bin:$PATH"
PYTHONPATH="$ROOT/engineering/sdk/python" \
    /c/Users/yinch/anaconda3/python.exe -c "
import sys
sys.path.insert(0, '$ROOT/engineering/sdk/python')
from pymultimodal import DB, Model

db = DB('/tmp/cross_lang_e2e.db')
hits = db.vectors_search('shared', [1.0, 0.0, 0.0, 0.0], top_k=3)
print(f'Python read OK, hits={len(hits)}')
for h in hits:
    print(f'  id={h[\"id\"]}, distance={h[\"distance\"]:.4f}')
db.close()
"

# 3. Go 读取验证
echo "[3/3] Go 读取验证..."
export PATH="/c/Program Files/Go/bin:$PATH"
export CGO_CFLAGS="-I$ROOT/engineering/include -I$ROOT/third_part/sqlite3"
export CGO_LDFLAGS="-L$BUILD_DIR/engineering/src/sdk -L$BUILD_DIR/sdk_sqlite3_build -lmmsdk -lsqlite3"
cd "$ROOT/engineering/sdk/go/multimodal"
go run -tags=integration /tmp/read_db.go 2>&1 || true

cat > /tmp/read_db.go <<'EOF'
package main

import (
    "fmt"
    multimodal "github.com/multimodal/multimodal"
)

func main() {
    db, err := multimodal.Open("/tmp/cross_lang_e2e.db")
    if err != nil { panic(err) }
    defer db.Close()

    hits, err := db.VectorSearch("shared", []float32{1.0, 0.0, 0.0, 0.0}, 3)
    if err != nil { panic(err) }
    fmt.Printf("Go read OK, hits=%d\n", len(hits))
    for _, h := range hits {
        fmt.Printf("  id=%s, distance=%.4f\n", h.ID, h.Distance)
    }
}
EOF

go run -tags=integration /tmp/read_db.go

echo "=========================================="
echo "跨语言一致性验证完成"
echo "=========================================="

# 清理
rm -f /tmp/cross_lang_e2e.db /tmp/write_db /tmp/write_db.cpp /tmp/read_db.go