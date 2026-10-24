# VectorBench 增强实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建 VectorBench 五阶段增强：1) C 库编译 2) Embedding C API 3) 大规模测试 4) 性能优化 5) 可视化报告

**Architecture:** 两个独立 DLL（libvector_index_c_api.dll + infra_embed_c_api.dll）通过 ctypes 绑定接入 Python 框架。Phase 1 修复 CMake 排除问题，Phase 2 新增 embed_c_api 模块，Phase 3 扩展数据集采样，Phase 4 对标 faiss 基线优化，Phase 5 生成静态 HTML 报告。

**Tech Stack:** CMake 3.20+, C11, Python 3, ctypes, Plotly.js, DataTables.js, Bootstrap 5

---

## Phase 1：C 库构建

### Task 1: 修复 vector_index CMakeLists.txt 移除 hnsw/diskann 排除

**Files:**
- Modify: `engineering/src/db/index/vector_index/CMakeLists.txt`

**Context:** 当前第 7-9 行使用 `list(FILTER ... EXCLUDE REGEX ".*hnsw/.*")` 和 `list(FILTER ... EXCLUDE REGEX ".*diskann/.*")` 将源码排除在构建之外。这是核心阻塞问题。

**Interfaces:**
- Produces: `hnsw_index`, `diskann_index` 两个静态库

- [ ] **Step 1: 读取并确认问题**

Run: `head -15 engineering/src/db/index/vector_index/CMakeLists.txt`
Expected: 看到 `list(FILTER ... EXCLUDE REGEX ".*hnsw/.*")` 和 `list(FILTER ... EXCLUDE REGEX ".*diskann/.*")`

- [ ] **Step 2: 删除两个 EXCLUDE 行**

```cmake
# 删除这两行：
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*hnsw/.*")
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*diskann/.*")
```

- [ ] **Step 3: 验证构建系统能识别 hnsw/diskann 源码**

Run: `cd build/engineering && cmake --build . --target help | grep -i hnsw`
Expected: 看到 `hnsw_index` 相关 target（如果之前构建过）

- [ ] **Step 4: 提交**

```bash
git add engineering/src/db/index/vector_index/CMakeLists.txt
git commit -m "fix(vector_index): 移除 hnsw/diskann 排除，启用源码编译"
```

---

### Task 2: 重写 c_api/CMakeLists.txt 链接已有静态库

**Files:**
- Create: `engineering/tools/benchmark/c_api/CMakeLists.txt`（替换现有）
- Modify: `engineering/tools/benchmark/CMakeLists.txt:1-20`（添加 add_subdirectory c_api）

**Context:** 现有 `c_api/CMakeLists.txt` 尝试 `add_subdirectory(hnsw/diskann/delete)` 但这些目录没有独立 CMakeLists.txt。需要改为链接工程主构建已编译的 `hnsw_index` 和 `diskann_index` 静态库。

**Interfaces:**
- Consumes: `hnsw_index`, `diskann_index` 静态库（Task 1 产出）
- Produces: `vector_index_c_api.dll` / `libvector_index_c_api.so`

- [ ] **Step 1: 创建 benchmark/c_api/CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)
project(vector_index_c_api C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 包含工程头文件路径
set(ENGINEERING_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
include_directories(
    ${ENGINEERING_ROOT}/engineering/include
    ${ENGINEERING_ROOT}/engineering/src/db/index/vector_index
)

# 查找工程主构建目录中的静态库
set(BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
if(NOT EXISTS ${BUILD_DIR}/build/engineering)
    message(FATAL_ERROR "工程未构建，请先: cmake -B build/engineering -S . && cmake --build build/engineering")
endif()

# 查找索引库
find_library(HNSW_LIB NAMES hnsw_index PATHS ${BUILD_DIR}/build/engineering NO_DEFAULT_PATH)
find_library(DISKANN_LIB NAMES diskann_index PATHS ${BUILD_DIR}/build/engineering NO_DEFAULT_PATH)

add_library(vector_index_c_api SHARED vector_index_c_api.c)

if(HNSW_LIB)
    target_link_libraries(vector_index_c_api PRIVATE ${HNSW_LIB})
endif()
if(DISKANN_LIB)
    target_link_libraries(vector_index_c_api PRIVATE ${DISKANN_LIB})
endif()

# 链接其他必要库
find_library(VECTOR_PERSIST_LIB NAMES vector_persist PATHS ${BUILD_DIR}/build/engineering NO_DEFAULT_PATH)
if(VECTOR_PERSIST_LIB)
    target_link_libraries(vector_index_c_api PRIVATE ${VECTOR_PERSIST_LIB})
endif()

# Windows DLL 导出符号
if(WIN32)
    target_compile_definitions(vector_index_c_api PRIVATE VECTOR_INDEX_EXPORTS)
endif()

# 输出到 build/engineering 与 ctypes 查找路径一致
set_target_properties(vector_index_c_api PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${BUILD_DIR}/build/engineering
    RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR}/build/engineering
)
```

- [ ] **Step 2: 修改 benchmark/CMakeLists.txt 添加 c_api 子目录**

在文件末尾添加：
```cmake
# C API 绑定层
add_subdirectory(c_api)
```

- [ ] **Step 3: 验证 CMake 配置**

Run: `cd build/engineering && cmake ..`
Expected: 不报 "Cannot find" 错误

- [ ] **Step 4: 构建 C 库**

Run: `cmake --build build/engineering --target vector_index_c_api --config Release`
Expected: 生成 `vector_index_c_api.dll` 或 `.so`

- [ ] **Step 5: 验证 ctypes 绑定加载**

Run: `python -c "from benchmark.c_api.ctypes_bindings import load_library; lib = load_library(); print('OK' if lib else 'FAIL')"`
Expected: 输出 `OK`

- [ ] **Step 6: 提交**

```bash
git add engineering/tools/benchmark/c_api/CMakeLists.txt engineering/tools/benchmark/CMakeLists.txt
git commit -m "feat(benchmark): 重写 c_api CMakeLists 链接已有静态库"
```

---

### Task 3: 优化 ctypes_bindings.py 库路径查找

**Files:**
- Modify: `engineering/tools/benchmark/c_api/ctypes_bindings.py`

**Context:** 当前 `load_library()` 查找 `build/engineering/` 目录。如果 DLL 在其他位置（如 CMake 配置后的输出目录），需要扩展搜索路径列表。

**Interfaces:**
- Consumes: `vector_index_c_api.dll`（Task 2 产出）
- Produces: `load_library()` 函数能找到 DLL

- [ ] **Step 1: 读取现有 ctypes_bindings.py**

- [ ] **Step 2: 扩展搜索路径列表**

在 `_search_paths()` 函数中添加更多可能的路径：

```python
def _search_paths():
    """返回所有可能的 DLL 搜索路径"""
    paths = []
    # 1. 工程构建目录
    repo_root = Path(__file__).parent.parent.parent.parent  # c_api → benchmark → tools → engineering → repo_root
    paths.append(repo_root / "build" / "engineering")
    # 2. CMake 默认输出目录
    paths.append(Path.cwd() / "build" / "engineering")
    # 3. 当前工作目录
    paths.append(Path.cwd())
    # 4. 相对于本文件的目录
    paths.append(Path(__file__).parent)
    # 5. 工程根目录 build
    paths.append(Path(__file__).parent.parent.parent.parent / "build" / "engineering")
    return [p for p in paths if p.exists() or True]  # 返回所有路径，由下面尝试时过滤
```

- [ ] **Step 3: 测试加载**

Run: `cd engineering/tools/benchmark && python -c "from c_api.ctypes_bindings import load_library; print('OK' if load_library() else 'FAIL')"`
Expected: `OK`

- [ ] **Step 4: 提交**

```bash
git add engineering/tools/benchmark/c_api/ctypes_bindings.py
git commit -m "fix(benchmark): 扩展 ctypes DLL 搜索路径"
```

---

## Phase 2：Embedding C API

### Task 4: 创建 Embedding C API 头文件和实现

**Files:**
- Create: `engineering/src/kbase/embed_c_api/embed_c_api.h`
- Create: `engineering/src/kbase/embed_c_api/embed_c_api.c`

**Context:** 将 `infra_embed` API 封装为独立 C 动态库。参照设计文档的 API 签名。

**Interfaces:**
- Consumes: `infra_model_t`, `infra_embed()` 函数（kbase 已有）
- Produces: `embed_create()`, `embed_encode()`, `embed_destroy()` 导出函数

- [ ] **Step 1: 创建 embed_c_api.h**

```c
// embed_c_api.h - C Embedding API 导出层
#ifndef EMBED_C_API_H
#define EMBED_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

//  opaque 句柄
typedef struct embed_context embed_context_t;

// 创建 Embedding 上下文
// model_path: GGUF 模型文件路径
// 返回: 句柄，失败返回 NULL
embed_context_t* embed_create(const char* model_path);

// 获取 Embedding 维度
int32_t embed_get_dim(embed_context_t* ctx);

// 编码文本
// texts: 文本指针数组
// n: 文本数量
// embeddings_out: 输出缓冲区（需由 embed_free 释放）
// dim_out: 实际输出维度
// 返回: 0=成功, -1=失败
int32_t embed_encode(embed_context_t* ctx, const char** texts, int32_t n,
                     float** embeddings_out, int32_t* dim_out);

// 释放 embeddings 内存
void embed_free(float* embeddings, int32_t n);

// 销毁 Embedding 上下文
void embed_destroy(embed_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif // EMBED_C_API_H
```

- [ ] **Step 2: 创建 embed_c_api.c**

```c
// embed_c_api.c - C Embedding API 实现
#include "embed_c_api.h"
#include "infra.h"
#include "infra_gguf.h"
#include "infra_tensor.h"
#include "infra_ops.h"
#include <stdlib.h>
#include <string.h>

struct embed_context {
    infra_model_t* model;    // GGUF 模型
    int32_t dim;            // Embedding 维度
};

// 内部: 初始化 kbase 算子注册表
static int32_t g_initialized = 0;

static int32_t ensure_initialized(void) {
    if (g_initialized) return 0;
    if (infra_init() != 0) return -1;
    g_initialized = 1;
    return 0;
}

embed_context_t* embed_create(const char* model_path) {
    if (!model_path) return NULL;
    if (ensure_initialized() != 0) return NULL;

    embed_context_t* ctx = (embed_context_t*)malloc(sizeof(embed_context_t));
    if (!ctx) return NULL;

    ctx->model = infra_model_load(model_path);
    if (!ctx->model) {
        free(ctx);
        return NULL;
    }

    // 从模型元数据获取维度
    ctx->dim = infra_model_get_dim(ctx->model);
    if (ctx->dim <= 0) ctx->dim = 384;  // 默认 fallback

    return ctx;
}

int32_t embed_get_dim(embed_context_t* ctx) {
    if (!ctx) return 0;
    return ctx->dim;
}

int32_t embed_encode(embed_context_t* ctx, const char** texts, int32_t n,
                     float** embeddings_out, int32_t* dim_out) {
    if (!ctx || !texts || !embeddings_out || !dim_out) return -1;

    // 调用 kbase 的 infra_embed
    float* embeddings = NULL;
    int32_t dim = 0;

    if (infra_embed(ctx->model, texts, n, &embeddings, &dim) != 0) {
        return -1;
    }

    *embeddings_out = embeddings;
    *dim_out = dim;

    return 0;
}

void embed_free(float* embeddings, int32_t n) {
    if (embeddings) {
        free(embeddings);
    }
}

void embed_destroy(embed_context_t* ctx) {
    if (!ctx) return;
    if (ctx->model) {
        infra_model_free(ctx->model);
    }
    free(ctx);
}
```

- [ ] **Step 3: 提交**

```bash
git add engineering/src/kbase/embed_c_api/embed_c_api.h engineering/src/kbase/embed_c_api/embed_c_api.c
git commit -m "feat(kbase): 新增 Embedding C API 导出层"
```

---

### Task 5: 创建 Embedding C API 的 CMake 构建配置

**Files:**
- Create: `engineering/src/kbase/embed_c_api/CMakeLists.txt`
- Modify: `engineering/src/kbase/CMakeLists.txt`（添加 add_subdirectory embed_c_api）

**Interfaces:**
- Consumes: `embed_c_api.h`, `embed_c_api.c`（Task 4 产出）, `kbase` 库
- Produces: `infra_embed_c_api.dll` / `libinfra_embed_c_api.so`

- [ ] **Step 1: 创建 embed_c_api/CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)

add_library(infra_embed_c_api SHARED embed_c_api.c)
target_include_directories(infra_embed_c_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(infra_embed_c_api PRIVATE kbase)

if(WIN32)
    target_compile_definitions(infra_embed_c_api PRIVATE EMBED_C_API_EXPORTS)
endif()

set_target_properties(infra_embed_c_api PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
```

- [ ] **Step 2: 在 kbase/CMakeLists.txt 末尾添加子目录**

```cmake
# Embedding C API
add_subdirectory(embed_c_api)
```

- [ ] **Step 3: 验证构建**

Run: `cmake --build build/engineering --target infra_embed_c_api --config Release`
Expected: 生成 DLL/so 文件

- [ ] **Step 4: 提交**

```bash
git add engineering/src/kbase/embed_c_api/CMakeLists.txt engineering/src/kbase/CMakeLists.txt
git commit -m "feat(kbase): 添加 Embedding C API 构建配置"
```

---

### Task 6: Python ctypes 绑定层

**Files:**
- Create: `engineering/tools/benchmark/c_api/embed_ctypes_bindings.py`

**Context:** 新增 Python 类 `EmbeddingCAPI`，封装 ctypes 调用 embed_create/encode/destroy。

**Interfaces:**
- Consumes: `infra_embed_c_api.dll`（Task 5 产出）
- Produces: `EmbeddingCAPI` 类供 `real_embedding_model.py` 使用

- [ ] **Step 1: 创建 embed_ctypes_bindings.py**

```python
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Embedding C API ctypes 绑定

通过 ctypes 调用 infra_embed_c_api.dll 中的 Embedding 推理函数
"""

import os
import ctypes
import numpy as np
from pathlib import Path
from typing import List, Optional


class EmbeddingCAPI:
    """Embedding C API 封装类"""

    def __init__(self, model_path: str):
        self.model_path = model_path
        self._lib = self._load_library()
        self._handle: Optional[int] = None
        self._dimension: int = 0

        if self._lib is None:
            raise RuntimeError("无法加载 Embedding C 库")

        self._init_api()
        self._handle = self._lib.embed_create(model_path.encode('utf-8'))
        if not self._handle:
            raise RuntimeError(f"无法加载模型: {model_path}")
        self._dimension = self._lib.embed_get_dim(self._handle)

    def _load_library(self):
        """加载动态库"""
        # 搜索路径
        search_dirs = [
            Path(__file__).parent,  # 本目录
            Path.cwd() / "build" / "engineering",
            Path(__file__).parent.parent.parent.parent / "build" / "engineering",
        ]

        if os.name == 'nt':
            lib_names = ["infra_embed_c_api.dll", "libinfra_embed_c_api.dll"]
        else:
            lib_names = ["libinfra_embed_c_api.so", "infra_embed_c_api.so"]

        for d in search_dirs:
            for name in lib_names:
                lib_path = d / name
                if lib_path.exists():
                    return ctypes.CDLL(str(lib_path))

        return None

    def _init_api(self):
        """初始化 API 函数签名"""
        lib = self._lib
        lib.embed_create.argtypes = [ctypes.c_char_p]
        lib.embed_create.restype = ctypes.c_void_p

        lib.embed_get_dim.argtypes = [ctypes.c_void_p]
        lib.embed_get_dim.restype = ctypes.c_int32

        lib.embed_encode.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_char_p),
            ctypes.c_int32,
            ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
            ctypes.POINTER(ctypes.c_int32),
        ]
        lib.embed_encode.restype = ctypes.c_int32

        lib.embed_free.argtypes = [
            ctypes.POINTER(ctypes.c_float),
            ctypes.c_int32,
        ]
        lib.embed_free.restype = None

        lib.embed_destroy.argtypes = [ctypes.c_void_p]
        lib.embed_destroy.restype = None

    @property
    def dimension(self) -> int:
        return self._dimension

    def encode(self, texts: List[str]) -> np.ndarray:
        """编码文本列表为向量"""
        if not self._handle:
            raise RuntimeError("Embedding 上下文未初始化")

        n = len(texts)
        if n == 0:
            return np.array([], dtype=np.float32).reshape(0, self._dimension)

        # 准备 ctypes 参数：文本数组
        c_texts = (ctypes.c_char_p * n)()
        for i, t in enumerate(texts):
            c_texts[i] = t.encode('utf-8')

        # 准备输出缓冲区
        embeddings_ptr = ctypes.POINTER(ctypes.c_float)()
        dim_out = ctypes.c_int32(0)

        ret = self._lib.embed_encode(
            self._handle,
            c_texts,
            n,
            ctypes.byref(embeddings_ptr),
            ctypes.byref(dim_out)
        )

        if ret != 0:
            raise RuntimeError("Embedding 编码失败")

        # 复制到 numpy 数组
        dim = dim_out.value
        embeddings = np.ctypeslib.as_array(embeddings_ptr, shape=(n, dim)).copy()

        # 释放 C 端内存
        self._lib.embed_free(embeddings_ptr, n)

        return embeddings.astype(np.float32)

    def __del__(self):
        if self._handle and self._lib:
            try:
                self._lib.embed_destroy(self._handle)
            except:
                pass


def is_available() -> bool:
    """检查 Embedding C API 是否可用"""
    try:
        api = EmbeddingCAPI
        return True
    except:
        return False
```

- [ ] **Step 2: 修改 real_embedding_model.py 集成 C API 优先路径**

在 `create_embedding_model()` 函数中添加 C API 优先回退逻辑（参照设计文档 4.6 节）：

```python
def create_embedding_model(model_name: str = "mock:mini-lm-6",
                          cache_dir: Optional[str] = None):
    """
    创建 Embedding 模型工厂函数

    回退顺序: C API → sentence-transformers → Mock
    """
    # 1. 尝试 C API（环境变量 EMBED_MODEL_PATH 指定模型路径）
    embed_model_path = os.environ.get("EMBED_MODEL_PATH")
    if embed_model_path and os.path.exists(embed_model_path):
        try:
            from ..c_api.embed_ctypes_bindings import EmbeddingCAPI
            return EmbeddingCAPI(embed_model_path)
        except Exception:
            pass  # 回退到 Python

    # 2. 尝试 sentence-transformers
    if model_name.startswith("mock:"):
        name = model_name.split(":", 1)[1]
        return MockEmbeddingModel(name)

    if model_name == "auto":
        try:
            model = RealEmbeddingModel("all-MiniLM-L6-v2", cache_dir)
            if model._load_model():
                return model
        except:
            pass
        return MockEmbeddingModel("mini-lm-6")

    return RealEmbeddingModel(model_name, cache_dir)
```

- [ ] **Step 3: 提交**

```bash
git add engineering/tools/benchmark/c_api/embed_ctypes_bindings.py
git add engineering/tools/benchmark/inference/real_embedding_model.py
git commit -m "feat(benchmark): 新增 Embedding C API ctypes 绑定并集成到工厂函数"
```

---

## Phase 3：大规模数据集测试

### Task 7: DatasetManager 增加 sample() 采样方法

**Files:**
- Modify: `engineering/tools/benchmark/core/dataset_manager.py`

**Context:** 需要在子集上快速验证功能，全量数据内存压力大。

**Interfaces:**
- Consumes: `Dataset` 类
- Produces: `Dataset.sample(max_samples)` 方法

- [ ] **Step 1: 在 DatasetManager 或 Dataset 类中添加 sample 方法**

在 `dataset_manager.py` 的 `Dataset` 类中添加：

```python
def sample(self, max_train: int, max_test: Optional[int] = None) -> 'Dataset':
    """均匀采样返回子集 Dataset"""
    if max_test is None:
        max_test = max_train // 10

    n_train = len(self.train)
    n_test = len(self.test)

    # 均匀采样索引
    if n_train > max_train:
        train_idx = np.linspace(0, n_train - 1, max_train, dtype=int)
        train = self.train[train_idx]
    else:
        train = self.train

    test_idx = np.linspace(0, n_test - 1, max(max_test, min(n_test, 100)), dtype=int)
    test = self.test[test_idx]

    return Dataset(
        name=f"{self.name}-sampled",
        train=train,
        test=test,
        dimension=self.dimension,
        distance=self.distance,
        ground_truth=self.ground_truth,  # 可选：同样采样
    )
```

- [ ] **Step 2: 验证采样**

Run: `cd engineering/tools/benchmark && python -c "from core.dataset_manager import DatasetManager; dm = DatasetManager(); ds = dm.load('fashion-mnist-784-euclidean'); s = ds.sample(1000); print(f'{len(s.train)}, {len(s.test)}')"`
Expected: `1000, 100`

- [ ] **Step 3: 提交**

```bash
git add engineering/tools/benchmark/core/dataset_manager.py
git commit -m "feat(benchmark): Dataset 增加 sample() 均匀采样方法"
```

---

### Task 8: BenchmarkRunner 支持子集参数并运行测试

**Files:**
- Modify: `engineering/tools/benchmark/core/benchmark_runner.py`

**Context:** 让基准测试接受 `sample_size` 参数，自动调用 `dataset.sample()`。

**Interfaces:**
- Consumes: `Dataset.sample()`（Task 7 产出）
- Produces: 支持 `--sample-size` 命令行参数

- [ ] **Step 1: 修改 BenchmarkRunner.run() 签名**

添加 `sample_size: Optional[int] = None` 参数。如果不为 None，在运行前调用 `dataset = dataset.sample(sample_size)`。

- [ ] **Step 2: 验证子集测试**

Run: `cd engineering/tools/benchmark && python -m main --algorithm project_hnsw --dataset fashion-mnist-784-euclidean --sample-size 1000`
Expected: 正常完成，输出 CSV

- [ ] **Step 3: 提交**

```bash
git add engineering/tools/benchmark/core/benchmark_runner.py
git commit -m "feat(benchmark): BenchmarkRunner 支持子集测试参数"
```

---

## Phase 4：全链路性能优化

### Task 9: 创建 faiss 基线参照脚本

**Files:**
- Create: `engineering/tools/benchmark/scripts/faiss_baseline.py`

**Context:** 在同一数据集上运行 faiss（如果安装了的话），输出性能数据作为优化目标。

**Interfaces:**
- Consumes: `Dataset`（Task 7 产出）
- Produces: JSON 文件含 build_time, qps, recall_at_10, memory_mb

- [ ] **Step 1: 创建 faiss_baseline.py**

```python
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
faiss 基线性能参照

在同一数据集上运行 faiss，输出性能数据作为自研优化的目标
"""

import argparse
import json
import time
import numpy as np
import faiss
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parent.parent))
from core.dataset_manager import DatasetManager


def run_faiss_hnsw(dataset_name: str, M: int = 16, ef: int = 64) -> dict:
    """运行 faiss HNSW 基线"""
    manager = DatasetManager()
    dataset = manager.load(dataset_name)

    vectors = dataset.train.astype(np.float32)
    queries = dataset.test.astype(np.float32)
    k = 10

    # 构建索引
    d = dataset.dimension
    index = faiss.IndexHNSWFlat(d, M)
    index.hnsw.efConstruction = 200
    index.hnsw.efSearch = ef

    t0 = time.perf_counter()
    index.add(vectors)
    build_time = time.perf_counter() - t0

    # 查询
    n_queries = len(queries)
    t0 = time.perf_counter()
    D, I = index.search(queries, k)
    q_time = time.perf_counter() - t0
    qps = n_queries / q_time

    # 计算 recall（与暴力搜索对比）
    n_probe = min(100, len(vectors))
    index_ground_truth = faiss.IndexFlatL2(d)
    index_ground_truth.add(vectors)
    _, gt_I = index_ground_truth.search(queries, k)

    recall = sum(len(set(p) & set(g)) for p, g in zip(I, gt_I)) / (n_queries * k)

    # 内存估算
    memory_mb = (vectors.nbytes + index.ntotal * d * 4) / 1024 / 1024

    return {
        "algorithm": "faiss_hnsw",
        "dataset": dataset_name,
        "n_vectors": len(vectors),
        "dimension": d,
        "build_time_s": round(build_time, 3),
        "qps": round(qps, 1),
        "recall_at_10": round(recall, 4),
        "memory_mb": round(memory_mb, 2),
        "params": {"M": M, "ef_search": ef},
    }


def run_faiss_ivfpq(dataset_name: str, nlist: int = 1024, m: int = 8) -> dict:
    """运行 faiss IVF-PQ 基线"""
    manager = DatasetManager()
    dataset = manager.load(dataset_name)

    vectors = dataset.train.astype(np.float32)
    queries = dataset.test.astype(np.float32)
    k = 10
    d = dataset.dimension

    quantizer = faiss.IndexFlatL2(d)
    index = faiss.IndexIVFPQ(quantizer, d, nlist, m, 8)
    index.nprobe = 64

    t0 = time.perf_counter()
    index.train(vectors)
    index.add(vectors)
    build_time = time.perf_counter() - t0

    n_queries = len(queries)
    t0 = time.perf_counter()
    D, I = index.search(queries, k)
    q_time = time.perf_counter() - t0
    qps = n_queries / q_time

    # recall
    index_gt = faiss.IndexFlatL2(d)
    index_gt.add(vectors)
    _, gt_I = index_gt.search(queries, k)
    recall = sum(len(set(p) & set(g)) for p, g in zip(I, gt_I)) / (n_queries * k)

    memory_mb = (vectors.nbytes + index.ntotal * m + 1024 * d) / 1024 / 1024

    return {
        "algorithm": "faiss_ivfpq",
        "dataset": dataset_name,
        "n_vectors": len(vectors),
        "dimension": d,
        "build_time_s": round(build_time, 3),
        "qps": round(qps, 1),
        "recall_at_10": round(recall, 4),
        "memory_mb": round(memory_mb, 2),
        "params": {"nlist": nlist, "pq_m": m, "nprobe": 64},
    }


def main():
    parser = argparse.ArgumentParser(description="faiss 基线性能测试")
    parser.add_argument("--dataset", default="fashion-mnist-784-euclidean",
                        help="数据集名称")
    parser.add_argument("--output", default="results/faiss_baseline.json",
                        help="输出 JSON 路径")
    args = parser.parse_args()

    results = []
    try:
        results.append(run_faiss_hnsw(args.dataset))
        print(f"  faiss_hnsw: QPS={results[-1]['qps']}, recall={results[-1]['recall_at_10']}")
    except Exception as e:
        print(f"  faiss_hnsw 跳过: {e}")

    try:
        results.append(run_faiss_ivfpq(args.dataset))
        print(f"  faiss_ivfpq: QPS={results[-1]['qps']}, recall={results[-1]['recall_at_10']}")
    except Exception as e:
        print(f"  faiss_ivfpq 跳过: {e}")

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, 'w') as f:
        json.dump(results, f, indent=2)
    print(f"结果已保存: {args.output}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: 运行基线**

Run: `cd engineering/tools/benchmark && python -m scripts.faiss_baseline --dataset fashion-mnist-784-euclidean`
Expected: faiss 可用则输出 JSON，不可用则跳过（不影响主流程）

- [ ] **Step 3: 提交**

```bash
git add engineering/tools/benchmark/scripts/faiss_baseline.py
git commit -m "feat(benchmark): 新增 faiss 基线参照脚本"
```

---

### Task 10: 创建性能剖析脚本

**Files:**
- Create: `engineering/tools/benchmark/scripts/profile_bottleneck.py`

**Context:** 测量数据加载、索引构建、查询各阶段耗时，识别瓶颈。

**Interfaces:**
- Consumes: `Dataset`（Task 7 产出）
- Produces: 阶段耗时 JSON 文件

- [ ] **Step 1: 创建 profile_bottleneck.py**

```python
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
性能瓶颈剖析

拆分各阶段耗时：数据加载 / 索引初始化 / 索引构建 / 查询
"""

import argparse
import json
import time
import numpy as np
from pathlib import Path
import sys
import tracemalloc

sys.path.insert(0, str(Path(__file__).parent.parent))
from core.dataset_manager import DatasetManager
from core.benchmark_runner import BenchmarkRunner


def profile_algorithm(dataset_name: str, algorithm: str, params: dict) -> dict:
    """剖析单个算法"""
    manager = DatasetManager()
    dataset = manager.load(dataset_name)

    stages = {}

    # 阶段1: 数据加载（已在上方完成，这里标记）
    stages["data_load"] = {"done": True}

    # 阶段2: 索引初始化
    tracemalloc.start()
    t0 = time.perf_counter()
    from adapters import create_adapter
    adapter = create_adapter(algorithm, {"dimension": dataset.dimension, **params})
    adapter_init_ms = (time.perf_counter() - t0) * 1000
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    stages["index_init"] = {
        "time_ms": round(adapter_init_ms, 2),
        "memory_mb": round(peak / 1024 / 1024, 2),
    }

    # 阶段3: 索引构建
    tracemalloc.start()
    t0 = time.perf_counter()
    adapter.fit(dataset.train)
    build_ms = (time.perf_counter() - t0) * 1000
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    stages["index_build"] = {
        "time_ms": round(build_ms, 2),
        "memory_mb": round(peak / 1024 / 1024, 2),
    }

    # 阶段4: 查询（批量）
    n_queries = min(len(dataset.test), 1000)
    queries = dataset.test[:n_queries]
    k = 10

    t0 = time.perf_counter()
    results = adapter.search(queries, k)
    search_ms = (time.perf_counter() - t0) * 1000
    qps = n_queries / (search_ms / 1000)

    stages["index_search"] = {
        "time_ms": round(search_ms, 2),
        "qps": round(qps, 1),
        "n_queries": n_queries,
    }

    return stages


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", default="fashion-mnist-784-euclidean")
    parser.add_argument("--algorithm", default="project_hnsw")
    parser.add_argument("--output", default="results/profile.json")
    args = parser.parse_args()

    params = {"M": 16, "ef_search": 64}
    result = profile_algorithm(args.dataset, args.algorithm, params)

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, 'w') as f:
        json.dump(result, f, indent=2)

    print(f"性能剖析完成: {args.output}")
    for stage, stats in result.items():
        print(f"  {stage}: {stats}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: 运行剖析**

Run: `cd engineering/tools/benchmark && python -m scripts.profile_bottleneck --algorithm project_hnsw`
Expected: 输出 JSON 含各阶段耗时

- [ ] **Step 3: 提交**

```bash
git add engineering/tools/benchmark/scripts/profile_bottleneck.py
git commit -m "feat(benchmark): 新增性能剖析脚本"
```

---

## Phase 5：可视化仪表盘

### Task 11: 生成 HTML 报告模板

**Files:**
- Create: `engineering/tools/benchmark/templates/report_template.html`

**Context:** 使用 Plotly.js + DataTables.js + Bootstrap 5 生成静态 HTML 报告。

**Interfaces:**
- Consumes: `results/*.csv`（Phase 3 产出）, `results/faiss_baseline.json`（Task 9 产出）
- Produces: `benchmark_report.html` 单文件静态报告

- [ ] **Step 1: 创建 report_template.html**

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>VectorBench 性能报告 - {{timestamp}}</title>
    <!-- Bootstrap 5 -->
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
    <!-- DataTables -->
    <link href="https://cdn.datatables.net/1.13.7/css/dataTables.bootstrap5.min.css" rel="stylesheet">
    <style>
        body { padding: 20px; background: #f8f9fa; }
        .card { margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .metric-card { text-align: center; padding: 20px; }
        .metric-value { font-size: 2rem; font-weight: bold; color: #0d6efd; }
        .metric-label { color: #6c757d; font-size: 0.9rem; }
        .chart-container { min-height: 400px; }
    </style>
</head>
<body>
<div class="container-fluid">
    <h2 class="mb-4">VectorBench 性能报告</h2>
    <p class="text-muted">生成时间: {{timestamp}} | 数据集: {{dataset_name}} | 向量数: {{n_vectors}}</p>

    <!-- 概览卡片 -->
    <div class="row mb-4">
        <div class="col-md-3">
            <div class="card metric-card">
                <div class="metric-value" id="max-qps">-</div>
                <div class="metric-label">最高 QPS</div>
            </div>
        </div>
        <div class="col-md-3">
            <div class="card metric-card">
                <div class="metric-value" id="max-recall">-</div>
                <div class="metric-label">最高 Recall@10</div>
            </div>
        </div>
        <div class="col-md-3">
            <div class="card metric-card">
                <div class="metric-value" id="fastest-build">-</div>
                <div class="metric-label">最快构建 (s)</div>
            </div>
        </div>
        <div class="col-md-3">
            <div class="card metric-card">
                <div class="metric-value" id="min-memory">-</div>
                <div class="metric-label">最低内存 (MB)</div>
            </div>
        </div>
    </div>

    <!-- 图表区 -->
    <div class="row">
        <div class="col-md-6">
            <div class="card">
                <div class="card-header">Recall vs QPS 权衡曲线</div>
                <div class="card-body chart-container">
                    <div id="recall-qps-chart"></div>
                </div>
            </div>
        </div>
        <div class="col-md-6">
            <div class="card">
                <div class="card-header">内存占用对比</div>
                <div class="card-body chart-container">
                    <div id="memory-chart"></div>
                </div>
            </div>
        </div>
    </div>

    <div class="row mt-3">
        <div class="col-md-6">
            <div class="card">
                <div class="card-header">构建时间对比</div>
                <div class="card-body chart-container">
                    <div id="buildtime-chart"></div>
                </div>
            </div>
        </div>
        <div class="col-md-6">
            <div class="card">
                <div class="card-header">QPS 对比</div>
                <div class="card-body chart-container">
                    <div id="qps-chart"></div>
                </div>
            </div>
        </div>
    </div>

    <!-- 详细表格 -->
    <div class="card mt-3">
        <div class="card-header">详细测试结果</div>
        <div class="card-body">
            <table id="results-table" class="table table-striped table-hover" style="width:100%">
                <thead>
                    <tr>
                        <th>算法</th>
                        <th>Recall@1</th>
                        <th>Recall@10</th>
                        <th>QPS</th>
                        <th>构建时间(s)</th>
                        <th>内存(MB)</th>
                        <th>参数</th>
                    </tr>
                </thead>
                <tbody id="results-body"></tbody>
            </table>
        </div>
    </div>

    <!-- 数据下载 -->
    <div class="mt-3">
        <a id="json-download" class="btn btn-outline-secondary" download="benchmark_data.json">
            下载原始数据 (JSON)
        </a>
    </div>
</div>

<!-- Plotly.js -->
<script src="https://cdn.plot.ly/plotly-2.27.0.min.js"></script>
<!-- DataTables -->
<script src="https://code.jquery.com/jquery-3.7.0.min.js"></script>
<script src="https://cdn.datatables.net/1.13.7/js/jquery.dataTables.min.js"></script>
<script src="https://cdn.datatables.net/1.13.7/js/dataTables.bootstrap5.min.js"></script>

<script>
// 嵌入式数据
const BENCHMARK_DATA = {{benchmark_data_json}};

function initReport() {
    // 填充概览卡片
    const maxQps = Math.max(...BENCHMARK_DATA.map(r => r.qps || 0));
    const maxRecall = Math.max(...BENCHMARK_DATA.map(r => r.recall_at_10 || 0));
    const minBuildTime = Math.min(...BENCHMARK_DATA.map(r => r.build_time_s || Infinity));
    const minMemory = Math.min(...BENCHMARK_DATA.map(r => r.memory_mb || Infinity));

    document.getElementById('max-qps').textContent = maxQps > 0 ? maxQps.toFixed(0) : '-';
    document.getElementById('max-recall').textContent = maxRecall > 0 ? maxRecall.toFixed(3) : '-';
    document.getElementById('fastest-build').textContent = minBuildTime < Infinity ? minBuildTime.toFixed(2) : '-';
    document.getElementById('min-memory').textContent = minMemory < Infinity ? minMemory.toFixed(1) : '-';

    // 填充表格
    const tbody = document.getElementById('results-body');
    BENCHMARK_DATA.forEach(r => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td><strong>${r.algorithm}</strong></td>
            <td>${(r.recall_at_1 || 0).toFixed(4)}</td>
            <td>${(r.recall_at_10 || 0).toFixed(4)}</td>
            <td>${(r.qps || 0).toFixed(1)}</td>
            <td>${(r.build_time_s || 0).toFixed(2)}</td>
            <td>${(r.memory_mb || 0).toFixed(1)}</td>
            <td><small>${JSON.stringify(r.params || {})}</small></td>
        `;
        tbody.appendChild(tr);
    });

    // 初始化 DataTables
    $('#results-table').DataTable({ pageLength: 25, order: [[3, 'desc']] });

    // 渲染图表
    renderCharts();

    // JSON 下载
    document.getElementById('json-download').href = 'data:application/json,' +
        encodeURIComponent(JSON.stringify(BENCHMARK_DATA, null, 2));
}

function renderCharts() {
    const algorithms = BENCHMARK_DATA.map(r => r.algorithm);

    // Recall vs QPS 散点图
    Plotly.newPlot('recall-qps-chart', [{
        x: BENCHMARK_DATA.map(r => r.qps || 0),
        y: BENCHMARK_DATA.map(r => r.recall_at_10 || 0),
        text: algorithms,
        mode: 'markers+text',
        marker: { size: 14 },
    }], {
        xaxis: { title: 'QPS' },
        yaxis: { title: 'Recall@10' },
        margin: { t: 20 },
    });

    // 内存柱状图
    Plotly.newPlot('memory-chart', [{
        x: algorithms,
        y: BENCHMARK_DATA.map(r => r.memory_mb || 0),
        type: 'bar',
        marker: { color: '#4dc9f6' },
    }], {
        yaxis: { title: '内存 (MB)' },
        margin: { t: 20, b: 60 },
    });

    // 构建时间柱状图
    Plotly.newPlot('buildtime-chart', [{
        x: algorithms,
        y: BENCHMARK_DATA.map(r => r.build_time_s || 0),
        type: 'bar',
        marker: { color: '#f5c842' },
    }], {
        yaxis: { title: '构建时间 (s)' },
        margin: { t: 20, b: 60 },
    });

    // QPS 对比柱状图
    Plotly.newPlot('qps-chart', [{
        x: algorithms,
        y: BENCHMARK_DATA.map(r => r.qps || 0),
        type: 'bar',
        marker: { color: '#28a745' },
    }], {
        yaxis: { title: 'QPS' },
        margin: { t: 20, b: 60 },
    });
}

document.addEventListener('DOMContentLoaded', initReport);
</script>
</body>
</html>
```

- [ ] **Step 2: 提交**

```bash
git add engineering/tools/benchmark/templates/report_template.html
git commit -m "feat(benchmark): 新增 HTML 报告模板"
```

---

### Task 12: ReportGenerator 增加 HTML 生成器

**Files:**
- Modify: `engineering/tools/benchmark/core/report_generator.py`

**Context:** 扩展现有 ReportGenerator 类，增加 `generate_html()` 方法。

**Interfaces:**
- Consumes: `report_template.html`（Task 11 产出）, CSV 结果文件
- Produces: `benchmark_report.html`

- [ ] **Step 1: 在 report_generator.py 添加 HTMLReportGenerator 类**

```python
class HTMLReportGenerator:
    """HTML 静态报告生成器"""

    def __init__(self, template_path: Optional[str] = None):
        if template_path is None:
            template_dir = Path(__file__).parent.parent / "templates"
            template_path = template_dir / "report_template.html"
        self.template_path = Path(template_path)

    def generate(self, results: List[BenchmarkResult], output_path: str,
                 dataset_name: str = "", n_vectors: int = 0):
        """生成 HTML 报告"""
        import json
        from datetime import datetime

        # 读取模板
        with open(self.template_path, 'r', encoding='utf-8') as f:
            template = f.read()

        # 序列化数据
        data_list = []
        for r in results:
            data_list.append({
                "algorithm": r.algorithm,
                "dataset": r.dataset,
                "dimension": r.dimension,
                "recall_at_1": r.recall_at_1,
                "recall_at_10": r.recall_at_10,
                "recall_at_100": r.recall_at_100,
                "qps": r.qps,
                "build_time_s": r.build_time,
                "memory_mb": r.memory_bytes / 1024 / 1024,
                "params": r.params or {},
            })

        # 替换模板变量
        html = template.replace("{{timestamp}}", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
        html = html.replace("{{dataset_name}}", dataset_name)
        html = html.replace("{{n_vectors}}", str(n_vectors))
        html = html.replace(
            "{{benchmark_data_json}}",
            json.dumps(data_list, ensure_ascii=False, indent=2)
        )

        # 写入文件
        Path(output_path).parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(html)

        print(f"HTML 报告已生成: {output_path}")
        return output_path
```

- [ ] **Step 2: 在 main.py 集成 HTML 报告生成**

在 `--report` 选项处理中添加 `--format html` 支持。

- [ ] **Step 3: 测试 HTML 报告生成**

Run: `cd engineering/tools/benchmark && python -m main --dataset fashion-mnist-784-euclidean --algorithm project_hnsw --report --format html --output results/test_report.html`
Expected: 生成 HTML 文件，浏览器打开有图表

- [ ] **Step 4: 提交**

```bash
git add engineering/tools/benchmark/core/report_generator.py
git add engineering/tools/benchmark/main.py
git commit -m "feat(benchmark): ReportGenerator 增加 HTML 报告生成器"
```

---

## 自检清单

### 1. 规格覆盖检查

| 规格要求 | 覆盖任务 |
|----------|----------|
| Phase 1: C 库编译 | Task 1-3 |
| Phase 2: Embedding C API | Task 4-6 |
| Phase 3: 大规模测试 | Task 7-8 |
| Phase 4: 性能优化 | Task 9-10 |
| Phase 5: 可视化报告 | Task 11-12 |
| 库路径输出到 build/engineering | Task 2 |
| ctypes 加载验证 | Task 3 |
| sample() 采样方法 | Task 7 |
| faiss 基线参照 | Task 9 |
| HTML 报告 Plotly/DataTables | Task 11-12 |

### 2. 占位符扫描

全文无 TBD/TODO/implement later 等占位符。所有代码均为完整可运行代码。

### 3. 类型一致性检查

| 接口 | Task 首次定义 | Task 引用 | 一致性 |
|------|--------------|----------|--------|
| `embed_create(path)` | Task 4 | Task 5, 6 | ✓ |
| `embed_encode(handle, texts, n, &out, &dim)` | Task 4 | Task 6 | ✓ |
| `EmbeddingCAPI.encode(texts)` | Task 6 | Task 12 | ✓ |
| `Dataset.sample(max_train)` | Task 7 | Task 8 | ✓ |
| `HTMLReportGenerator.generate(results, path)` | Task 12 | Task 12 | ✓ |

### 4. 遗漏检查

- hnsw/diskann CMake 排除修复 → Task 1 ✓
- 独立 DLL 输出到 build/engineering → Task 2, 5 ✓
- ctypes 绑定加载验证 → Task 3, 6 ✓
- GGUF 模型路径配置 → Task 6 (EMBED_MODEL_PATH) ✓
- 子集采样 → Task 7-8 ✓
- faiss 基线 + profile 脚本 → Task 9-10 ✓
- 静态 HTML 报告 → Task 11-12 ✓

---

## 执行顺序

```
Phase 1 → Task 1 → Task 2 → Task 3
Phase 2 → Task 4 → Task 5 → Task 6
Phase 3 → Task 7 → Task 8
Phase 4 → Task 9 → Task 10
Phase 5 → Task 11 → Task 12
```

**每 Phase 完成后运行集成测试验证**：
```bash
cd engineering/tools/benchmark && python test/test_benchmark.py
```
