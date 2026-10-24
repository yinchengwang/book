# RAG Web 功能扩展实现方案

> 日期：2026-09-03
> 仓库：`D:\code\book\engineering\rag`
> Web UI：`D:\code\book\engineering\web`

---

## 概述

本方案覆盖 4 大功能模块，按依赖关系排序：

```
Phase 1 ──→ Phase 2 ──→ Phase 3 ──→ Phase 4
  上传        PDF解析      重建索引     kbase接入
```

---

## Phase 1：文件上传与目录管理

### 任务 1：后端 — 文件上传接口

**文件**: `src/rag/server/server.cpp`

新增 `handle_upload()` 方法：

```
POST /api/v1/documents/upload
Content-Type: multipart/form-data

Fields:
  file        : 文件 binary（支持 .zip/.tar.gz 批量）
  target_dir  : string, 目标目录路径（如 "my-books/algorithms"）
                 默认 ""
Response:
{
  "uploaded": 3,
  "skipped": 1,
  "errors": [],
  "files": [
    { "name": "book.pdf", "size": 2048000, "status": "uploaded" },
    { "name": "notes.zip", "size": 512000, "status": "extracted", "count": 12 }
  ]
}
```

**实现要点**：
1. 解析 `multipart/form-data`（可复用轻量解析或引入 `mongoose` 的 mime 处理）
2. `.zip` 文件 → 解压到 `target_dir`，递归扫描解压内容
3. 单文件 → 直接写入 `./rag_data/{target_dir}/{filename}`
4. 检测重复：`hash(path + mtime)` 已在索引 → 跳过（不报错）
5. 上传后触发增量索引（可选，通过 `index_mode: "auto"` 或 `"manual"` 配置）

**错误处理**：
- 文件类型不支持 → `"status": "unsupported"`
- 磁盘满 → HTTP 413
- 解析失败 → `"status": "error"`, `"error": "parse failed"`

---

### 任务 2：后端 — 目录管理接口

**文件**: `src/rag/server/server.cpp`

```
POST   /api/v1/documents/dirs
Body:   { "path": "my-books/algorithms" }
Response: { "created": true, "path": "my-books/algorithms" }
        或 { "error": "path exists" }

DELETE /api/v1/documents/dirs?path=my-books/algorithms
Response: { "deleted": true }
        或 { "error": "directory not empty" } （可选：force=true 递归删除）

GET    /api/v1/documents/dirs
Response: { "dirs": [
  { "path": "my-books", "file_count": 5, "total_size": 10240000 },
  { "path": "my-books/algorithms", "file_count": 3, "total_size": 5120000 }
] }
```

**实现要点**：
1. `POST` → `std::filesystem::create_directories("./rag_data/" + path)`
2. `DELETE` → 检查目录是否为空，为空才删除（避免误删有内容的目录）
3. `GET` → 递归扫描 `./rag_data/` 下所有子目录，统计文件数和总大小

---

### 任务 3：后端 — 文档列表与删除

**文件**: `src/rag/server/server.cpp`

已有 `GET /api/v1/documents`，需增强：

```
GET /api/v1/documents?dir=my-books/algorithms
Response: {
  "documents": [
    {
      "id": "uuid-xxx",
      "name": "chapter1.pdf",
      "path": "my-books/algorithms/chapter1.pdf",
      "size": 2048000,
      "type": "pdf",
      "indexed_at": "2026-09-03T10:00:00Z",
      "chunk_count": 234
    }
  ],
  "dirs": ["my-books/algorithms/chapter2"]
}

DELETE /api/v1/documents/{id}
Response: { "deleted": true }
```

---

### 任务 4：后端 — 删除文档

增强 `handle_document()`，支持 `DELETE` 方法：
1. 根据 `id` 查找文档的 `file_path`
2. 删除文件系统上的文件
3. 从内存元数据 `unordered_map` 中移除
4. 从向量索引和 BM25 索引中移除（调用 `index_->remove(id)`）
5. 返回 `{ "deleted": true }`

---

### 任务 5：前端 — 文件管理面板

**文件**: `src/components/FileManager.tsx`（新建）

```
┌──────────────────────────────────────────────────┐
│ 📁 文件管理                              [⏹ 索引中] │
├──────────────────────────────────────────────────┤
│ [📂 新建文件夹 ▼]  [⬆ 上传 ▼]  [🔄 重建索引]       │
├──────────────────────────────────────────────────┤
│ 路径: /my-books/algorithms/          [↩ 返回上级] │
├──────────────────────────────────────────────────┤
│ 📂 chapter1/                      2026-09-03     │
│ 📄 introduction.md   45KB   ✅ 已索引            │
│ 📄 sorting.pdf      2.3MB  ✅ 已索引  (234块)   │
│ 📄 sorting.pdf      2.3MB  ⏳ 解析中...          │
│ 📄 sorting.pdf      2.3MB  ❌ 解析失败          │
├──────────────────────────────────────────────────┤
│ 共 3 个文件 | 总大小: 2.4MB                       │
└──────────────────────────────────────────────────┘
```

**组件状态**：
- `activeTab: "files"` 在 `App.tsx` 中切换
- 文件列表通过 `useQuery(['documents', currentDir], ...)` 获取
- 上传进度通过 `XMLHttpRequest` 的 `upload.onprogress` 实时更新

---

### 任务 6：前端 — 上传 UI

**文件**: `src/components/UploadDialog.tsx`（新建）

- 拖拽上传区域（`onDrop` 事件）
- 文件夹选择（`<input type="file" webkitdirectory>`）
- 目标目录下拉选择 + 新建文件夹输入框
- 上传进度条（实时百分比 + 当前处理文件名）
- zip 解压后文件列表预览

---

### 任务 7：前端 — 目录管理 UI

**文件**: `src/components/DirManager.tsx`（新建）

- 新建文件夹弹窗（输入路径 + 确认）
- 右键菜单（删除/重命名）
- 面包屑导航（点击切换目录）
- 目录树折叠面板（侧边栏）

---

## Phase 2：PDF 专业解析

### 架构决策：Python 扩展 vs 独立服务

| 方案 | 优点 | 缺点 |
|------|------|------|
| **pybind11 C++ 扩展** | 零网络开销，进程内调用 | 编译复杂，Tesseract 需找到系统库 |
| **独立 Python gRPC 服务** | 独立部署，语言栈纯粹 | 网络延迟，额外服务进程 |
| **✅ 推荐：Python HTTP 服务** | 简单，RAG 后端已有 HTTP 服务器能力 | 需启动独立端口 |

**决定**：Python HTTP 服务（`localhost:8081`），RAG C++ 后端通过 HTTP 调用。

```
RAG Server (8080)          PDF Parser (8081)
    │                            │
    │──POST /api/v1/documents/upload──→ │
    │                            │  POST /parse
    │                            │ ←── {text, chunks, tables, images}
    │──index chunks──→           │
```

---

### 任务 8：Python PDF 解析服务

**文件**: `D:\code\book\engineering\rag\pdf_parser\`

```
pdf_parser/
├── server.py              # HTTP 服务入口 (Flask/FastAPI)
├── parser/
│   ├── __init__.py
│   ├── extractor.py       # 文本提取 (pdfminer + PyMuPDF)
│   ├── ocr.py             # Tesseract OCR
│   ├── tables.py          # 表格提取 (pdfplumber)
│   ├── layout.py          # 版面分析
│   ├── postprocess.py     # 后处理（跨页合并等）
│   └── output.py          # 输出 MultipartDocument JSON
├── requirements.txt
└── run.sh                 # 启动脚本
```

**服务接口**：

```
POST http://localhost:8081/parse
Content-Type: application/json
Body: { "path": "/path/to/book.pdf", "options": { "ocr": true, "tables": true } }
Response: {
  "success": true,
  "page_count": 456,
  "text": "...",
  "chunks": [
    {
      "id": "uuid",
      "content": "第3章 排序算法...",
      "type": "text",          // "text" | "table" | "code" | "math"
      "page_start": 45,
      "page_end": 47,
      "heading": "第3章 排序算法"
    }
  ],
  "tables": [
    { "page": 123, "csv": "col1,col2\n..." }
  ],
  "images": [
    { "page": 50, "extracted_text": "图表描述..." }
  ],
  "metadata": {
    "title": "算法导论",
    "author": "Thomas H. Cormen",
    "toc": [{ "title": "...", "page": 45 }]
  }
}
```

**启动方式**：
```bash
# 在 rag 项目根目录下
python3 pdf_parser/server.py --port 8081
```

---

### 任务 9：OCR 集成（Tesseract）

**文件**: `pdf_parser/parser/ocr.py`

```python
def ocr_page(image_bytes: bytes) -> str:
    """将 PDF 页面转为图像后 OCR"""
    # 1. image_bytes → PIL Image
    img = Image.open(io.BytesIO(image_bytes))
    # 2. 灰度转换
    img = img.convert('L')
    # 3. DPI 标准化 (300 DPI)
    img = img.resize((w * 2, h * 2), Image.LANCZOS)
    # 4. 去噪 (可选: cv2.fastNlMeansDenoising)
    # 5. Tesseract OCR
    text = pytesseract.image_to_string(
        img,
        lang='chi_sim+eng',      # 中文+英文
        config='--psm 6'          # 保持段落结构
    )
    return text
```

**检测扫描页**：
```python
def is_scanned_page(pdf_path: str, page_num: int) -> bool:
    """文字提取成功率 < 30% → 判定为扫描页"""
    text = extract_text_only(pdf_path, page_num)
    return len(text.strip()) < 50
```

---

### 任务 10：表格提取

**文件**: `pdf_parser/parser/tables.py`

```python
def extract_tables(pdf_path: str, page_num: int) -> List[TableResult]:
    """尝试多种表格提取方法，取最优结果"""
    results = []

    # 方法1: pdfplumber（有边框表格）
    tables_pp = pdfplumber.open(pdf_path).pages[page_num].extract_tables()
    results.append(("pdfplumber", tables_pp))

    # 方法2: camelot（无边框/线检测表格）
    tables_cm = camelot.extract(pdf_path, pages=[str(page_num)])
    results.append(("camelot", tables_cm))

    # 合并: 取行数最多的结果
    best = max(results, key=lambda r: sum(len(t) for t in r[1]))
    return format_as_csv(best)
```

**跨页表格合并**：
```python
def merge_cross_page_tables(pages_tables: Dict[int, TableResult]) -> List[TableResult]:
    """合并相邻页的跨页表格"""
    merged = []
    i = 0
    while i < len(pages_tables):
        page, table = pages_tables[i]
        if i + 1 < len(pages_tables):
            next_page, next_table = pages_tables[i + 1]
            # 检查表头是否重复（跨页特征）
            if next_table.header == table.header:
                merged.append(merge_two_tables(table, next_table))
                i += 2
                continue
        merged.append(table)
        i += 1
    return merged
```

---

### 任务 11：版面分析（双栏排版）

**文件**: `pdf_parser/parser/layout.py`

```python
def detect_layout_mode(pdf_path: str, page_num: int) -> str:
    """检测版面模式: single_column / two_column / mixed"""
    page = fitz.open(pdf_path)[page_num]
    blocks = page.get_text("dict")["blocks"]

    # 统计 x 坐标分布
    x_coords = [b["bbox"][0] for b in blocks if b["type"] == 0]
    x_coords.sort()

    # 找双峰（双栏特征）
    if len(x_coords) > 10:
        gaps = [x_coords[i+1] - x_coords[i] for i in range(len(x_coords)-1)]
        large_gaps = [g for g in gaps if g > page.width * 0.2]
        if len(large_gaps) >= 2:
            return "two_column"

    return "single_column"


def reorder_two_column_text(blocks: List[dict]) -> List[dict]:
    """双栏文字按阅读顺序重排"""
    # 分左右栏
    left = [b for b in blocks if b["bbox"][0] < page_width / 2]
    right = [b for b in blocks if b["bbox"][0] >= page_width / 2]

    # 各自按 y 排序（从上到下）
    left.sort(key=lambda b: b["bbox"][1])
    right.sort(key=lambda b: b["bbox"][1])

    # 交替合并（先读左栏第1段，再右栏第1段...）
    reordered = []
    for l, r in zip(left, right):
        reordered.extend(sorted([l, r], key=lambda b: b["bbox"][1]))

    # 处理栏数不等的情况
    if len(left) > len(right):
        reordered.extend(left[len(right):])
    elif len(right) > len(left):
        reordered.extend(right[len(left):])

    return reordered
```

---

### 任务 12：后处理（跨页段落、代码块、章节层级）

**文件**: `pdf_parser/parser/postprocess.py`

```python
def merge_cross_page_paragraphs(pages_text: List[str]) -> List[str]:
    """合并被 page break 打断的段落"""
    merged = []
    current = ""
    for text in pages_text:
        stripped = text.strip()
        if current:
            # 检查 current 是否以不完整单词结尾
            if current[-1].isalpha() and stripped[0].isalpha():
                current += " " + stripped  # 跨页合并
            else:
                merged.append(current)
                current = stripped
        else:
            current = stripped
    if current:
        merged.append(current)
    return merged


def detect_code_blocks(text: str, font_info: List[dict]) -> List[CodeBlock]:
    """等宽字体区域 → 代码块"""
    code_regions = [f for f in font_info if is_monospace(f["font"])]
    # 合并相邻代码区域，保留原始缩进
    return merge_adjacent_code(code_regions)


def chunk_long_content(content: str, max_chars: int = 500) -> List[Chunk]:
    """长内容按逻辑边界切分"""
    if len(content) <= max_chars:
        return [Chunk(content=content)]

    # 文本: 按段落边界切
    # 代码: 按空行/函数边界切
    # 表格: 不切（表格作为独立块）
    chunks = []
    for segment in split_by_boundary(content):
        if len(segment) > max_chars:
            chunks.extend(chunk_long_content(segment, max_chars))
        else:
            chunks.append(Chunk(content=segment))
    return chunks
```

---

## Phase 3：重建索引

### 任务 13：后端 — 重建索引接口

**文件**: `src/rag/server/server.cpp`

```
POST /api/v1/index/rebuild
Body:   { "force": true }    // true = 删除旧索引重新构建
Response: { "job_id": "job-2026-09-03-001", "status": "started" }

GET  /api/v1/index/rebuild/status?job_id=job-2026-09-03-001
Response: {
  "job_id": "job-2026-09-03-001",
  "status": "running",        // "queued" | "running" | "completed" | "failed"
  "progress": 0.45,
  "total_files": 100,
  "processed_files": 45,
  "current_file": "my-books/algorithms/chapter1.pdf",
  "started_at": "2026-09-03T10:00:00Z",
  "message": "Parsing chapter1.pdf (234 pages)"
}

DELETE /api/v1/index/rebuild?job_id=job-2026-09-03-001
Response: { "cancelled": true }   // 取消运行中的任务
```

**实现要点**：
1. `POST` → 创建 `IndexJob` 对象，放入后台线程池，**立即返回** `job_id`
2. 后台线程执行：
   - 扫描 `./rag_data/` 所有文件
   - 对每个文件调用解析器（文本/PDF）
   - 分块 → embedding → `index_->add()`
   - 更新 `IndexJob` 进度（`progress_` 原子变量）
3. `GET status` → 读取 `IndexJob` 的进度快照
4. 重建前备份旧索引文件为 `hnsw.bin.bak`

**线程安全**：
```cpp
struct IndexJob {
    std::atomic<float> progress;
    std::atomic<int> processed_files;
    std::atomic<bool> cancelled;
    std::string current_file;
    std::thread worker;
};
```

---

### 任务 14：后端 — 增量索引支持

在文档上传/删除时支持增量更新，而不必全量重建：

```cpp
// handle_upload 中，上传成功后：
void index_document(const std::string& file_path) {
    auto doc = parser.parse(file_path);
    for (const auto& chunk : doc.chunks) {
        auto embedding = embed_service_->encode(chunk.content);
        index_->add(chunk.id, embedding);
        bm25_->add(chunk.id, chunk.content);
    }
}

// handle_delete 中，删除成功后：
void unindex_document(const std::string& doc_id) {
    index_->remove(doc_id);
    bm25_->remove(doc_id);
}
```

---

### 任务 15：前端 — 重建索引 UI

**文件**: `src/components/RebuildDialog.tsx`（新建）

```
┌─────────────────────────────────────────┐
│  🔄 重建索引                              │
├─────────────────────────────────────────┤
│  当前状态: 正在重建索引                   │
│  进度: ████████░░░░░░░░ 45%             │
│                                         │
│  当前处理: my-books/algorithms/          │
│            chapter1.pdf (234页)          │
│                                         │
│  已处理: 45 / 100 个文件                 │
│  预计剩余: 约 5 分钟                      │
├─────────────────────────────────────────┤
│  [取消]                      [后台运行]   │
└─────────────────────────────────────────┘
```

**实现要点**：
1. `POST /api/v1/index/rebuild` 后立即轮询 `GET /api/v1/index/rebuild/status`
2. 轮询间隔：初始 1s，进度 > 80% 时改为 3s（后期文件可能更大）
3. 完成后自动关闭对话框 + Toast 提示
4. 页面刷新后同步索引状态

---

## Phase 4：kbase 数据库接入

### 任务 16：KbaseRetriever 实现

**文件**: `include/rag/kbase_retriever.h`（新建）

```cpp
class KbaseRetriever : public Retriever {
public:
    KbaseRetriever(kbase_index_t* idx,
                   infra_model_t* model,
                   const RetrievalConfig& config);

    ~KbaseRetriever();

    std::vector<RetrievalResult> retrieve(
        const std::string& query, int top_k) override;

    std::string name() const override { return "kbase"; }
    const RetrievalConfig& config() const override { return config_; }

private:
    kbase_index_t* idx_;
    infra_model_t* model_;
    RetrievalConfig config_;
};
```

**文件**: `src/rag/kbase_retriever.cpp`（新建）

```cpp
#include <rag/kbase_retriever.h>

std::vector<RetrievalResult> KbaseRetriever::retrieve(
    const std::string& query, int top_k)
{
    int num_results = 0;
    kbase_result_t* results = kbase_search(
        idx_, model_, query.c_str(), top_k, &num_results);

    std::vector<RetrievalResult> ret;
    for (int i = 0; i < num_results; ++i) {
        RetrievalResult r;
        r.content = results[i].note->content;
        r.file_path = results[i].note->path;
        r.vector_score = 1.0f - results[i].score;  // kbase 返回距离，转为相似度
        r.rank = i;
        ret.push_back(r);
    }

    kbase_search_free(results, num_results);
    return ret;
}
```

---

### 任务 17：KbaseBM25Retriever 实现

**前置**：kbase 侧需先实现 BM25 检索接口。

```c
// kbase_index.h 新增
typedef struct {
    int note_id;
    float score;
} kbase_bm25_result_t;

kbase_bm25_result_t* kbase_bm25_search(
    kbase_index_t* idx,
    const char* query,
    int top_k,
    int* num_results);
```

**文件**: `include/rag/kbase_bm25_retriever.h` + `src/rag/kbase_bm25_retriever.cpp`

包装方式同 `KbaseRetriever`，映射 `score` 到 `bm25_score`。

---

### 任务 18：检索器工厂更新

**文件**: `src/rag/retriever.cpp`

```cpp
std::shared_ptr<Retriever> create_retriever(
    const RetrievalConfig& config,
    std::shared_ptr<VectorIndex> vector_index,
    std::shared_ptr<BM25Index> bm25_index,
    std::shared_ptr<EmbeddingService> embed_service)
{
    // 优先级1: 使用 kbase（向量 + BM25）
    if (config.use_kbase) {
        auto kbase_idx = kbase_index_create(data_dir.c_str());
        kbase_index_load(kbase_idx, kbase_index_path.string().c_str());

        auto model = infra_model_load(config.model_path.c_str());

        if (config.use_kbase_bm25) {
            // 全部用 kbase
            auto hnsw = std::make_shared<KbaseRetriever>(kbase_idx, model, config);
            auto bm25 = std::make_shared<KbaseBM25Retriever>(kbase_idx, config);
            return std::make_shared<HybridRetriever>(hnsw, bm25, config);
        } else {
            // 仅向量用 kbase，BM25 用 RAG 侧
            auto hnsw = std::make_shared<KbaseRetriever>(kbase_idx, model, config);
            auto bm25 = std::make_shared<BM25Retriever>(bm25_index, config);
            return std::make_shared<HybridRetriever>(hnsw, bm25, config);
        }
    }

    // 优先级2: 使用 RAG 侧原生实现
    auto hnsw = std::make_shared<HNSWRetriever>(vector_index, embed_service, config);
    auto bm25 = std::make_shared<BM25Retriever>(bm25_index, config);
    return std::make_shared<HybridRetriever>(hnsw, bm25, config);
}
```

---

## Phase 4b：数据库迁移框架

### T18b：迁移架构设计

**目标**：将 RAG 索引数据（向量 + BM25 + 元数据）导出为标准中间格式，导入到任意支持向量检索的数据库，实现零锁定。

#### 4b.1 迁移策略

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   源数据库    │     │   中间格式    │     │  目标数据库   │
│  (kbase)    │────▶│  (导出文件)   │────▶│  Milvus/     │
│  hnsw.bin   │     │  vectors.jsonl│     │  Weaviate/   │
│  bm25.bin   │     │  bm25.jsonl  │     │  Qdrant/...  │
└──────────────┘     │  meta.json   │     └──────────────┘
                     └──────────────┘
```

#### 4b.2 中间格式设计

**向量数据** `vectors.jsonl`（每行一个向量）：
```json
{"id": "uuid-001", "vector": [0.123, -0.456, ...], "content": "第3章 排序算法...", "metadata": {"file_path": "book.pdf", "page_start": 45, "page_end": 47, "type": "text", "heading": "第3章 排序算法", "indexed_at": "2026-09-03T10:00:00Z"}}
```

**BM25 数据** `bm25.jsonl`（每行一个文档，含预分词结果）：
```json
{"id": "uuid-001", "content": "第3章 排序算法...", "terms": ["排序", "算法", "时间复杂度", "归并排序"], "file_path": "book.pdf"}
```

**元数据** `meta.json`：
```json
{"version": "1.0", "exported_at": "2026-09-03T12:00:00Z", "source": "kbase", "total_vectors": 12345, "embedding_model": "MiniLM-L6", "embedding_dim": 384, "dirs": [{"path": "my-books", "file_count": 5}]}
```

#### 4b.3 迁移接口设计

```
# 导出
POST /api/v1/migration/export
Body: { "format": "jsonl", "include_vectors": true, "include_bm25": true }
Response: { "job_id": "export-001", "status": "started" }

GET /api/v1/migration/export/status?job_id=export-001
Response: { "status": "completed", "file": "./rag_data/exports/rag_export_20260903.tar.gz" }

GET /api/v1/migration/export/download?job_id=export-001
Response: [binary tar.gz]

# 导入
POST /api/v1/migration/import
Content-Type: multipart/form-data  (file: <uploaded tar.gz>)
Response: { "job_id": "import-001", "status": "started" }

GET /api/v1/migration/import/status?job_id=import-001
Response: { "status": "completed", "imported": 12345, "skipped": 0 }

# 迁移状态
GET /api/v1/migration/status
Response: { "current_db": "kbase", "available_dbs": ["kbase", "milvus", "weaviate", "qdrant", "native"] }
```

#### 4b.4 数据库适配器接口

```cpp
// include/rag/db_adapter.h
class VectorDBAdapter {
public:
    virtual ~VectorDBAdapter() = default;

    virtual bool connect(const DBConfig& config) = 0;
    virtual void disconnect() = 0;
    virtual bool upsert_vectors(const std::vector<VectorRecord>& records) = 0;
    virtual std::vector<SearchResult> search(
        const std::vector<float>& query_vec,
        int top_k,
        const SearchFilter& filter = {}) = 0;
    virtual bool delete_by_id(const std::string& id) = 0;
    virtual bool delete_by_filter(const SearchFilter& filter) = 0;
    virtual size_t count() const = 0;
    virtual bool health_check() const = 0;
    virtual std::string name() const = 0;
};

std::unique_ptr<VectorDBAdapter> create_db_adapter(
    const std::string& db_type,   // "kbase" | "milvus" | "weaviate" | "qdrant"
    const DBConfig& config);
```

#### 4b.5 适配器实现矩阵

| 适配器 | 通信方式 | 文件 |
|--------|----------|------|
| `KbaseAdapter` | 进程内调用 | `db_adapter_kbase.cpp` |
| `MilvusAdapter` | gRPC | `db_adapter_milvus.cpp` |
| `WeaviateAdapter` | REST API | `db_adapter_weaviate.cpp` |
| `QdrantAdapter` | gRPC | `db_adapter_qdrant.cpp` |

#### 4b.6 迁移执行流程

```
导出（kbase → 文件）:
  1. 加载 kbase_index，加读锁
  2. 遍历所有 notes → vectors.jsonl
  3. 导出 BM25 → bm25.jsonl
  4. 导出元数据 → meta.json
  5. 打包为 tar.gz（.gitignore 排除）
  6. 返回下载路径

导入（文件 → 目标 DB）:
  1. 解压 tar.gz，校验 schema 版本
  2. 构造目标 DB 的 collection/schema（如 Milvus 需先建 collection）
  3. 批量 upsert 向量（每批 1000 条）
  4. 支持断点续传（记录已导入的 id 列表，失败重试）
  5. 更新迁移元数据

一致性保证:
  - 导出加读锁（不影响查询）
  - 导入先写临时表，全部成功后切换指针
  - 两阶段提交：导出 → 验证 → 导入 → 回滚预案
```

#### 4b.7 配置变更

```yaml
# rag_config.yaml
database:
  current: "kbase"      # 切换数据库只需改这一行

  adapters:
    kbase:
      data_dir: "./rag_data"
      model_path: "./models/minilm-l6.bin"

    milvus:
      host: "localhost"
      port: 19530
      collection: "rag_chunks"
      index_type: "HNSW"
      metric: "L2"

    weaviate:
      url: "http://localhost:8080"
      class_name: "RagChunk"

    qdrant:
      url: "http://localhost:6333"
      collection: "rag_chunks"

  migration:
    export_dir: "./rag_data/exports"
    batch_size: 1000
    checkpoint_enabled: true
```

#### 4b.8 适配器注册与切换

```cpp
// src/rag/db_adapter_factory.cpp
std::unique_ptr<VectorDBAdapter> create_db_adapter(
    const std::string& db_type, const DBConfig& config)
{
    if (db_type == "kbase") {
        return std::make_unique<KbaseAdapter>(config);
    } else if (db_type == "milvus") {
        return std::make_unique<MilvusAdapter>(config);
    } else if (db_type == "weaviate") {
        return std::make_unique<WeaviateAdapter>(config);
    } else if (db_type == "qdrant") {
        return std::make_unique<QdrantAdapter>(config);
    }
    return nullptr;
}

// 切换数据库：只需改配置文件的 current 字段
// Retriever 通过 VectorDBAdapter 抽象层无感知切换
```

---

## Phase 5：集成与端到端验证

### 任务 19：Python PDF 解析服务注册

**文件**: `D:\code\book\engineering\rag\pdf_parser\requirements.txt`

```
pdfminer.six>=20231228
PyMuPDF>=1.23.0
pytesseract>=0.3.10
pdfplumber>=0.10.0
camelot-py>=0.10.1
opencv-python>=4.8.0
Pillow>=10.0.0
flask>=3.0.0          # 或 fastapi>=0.100.0 + uvicorn
```

**启动脚本**: `run_pdf_parser.sh`

```bash
#!/bin/bash
# 检查 Tesseract
if ! command -v tesseract &> /dev/null; then
    echo "Error: Tesseract not installed. Run: apt install tesseract-ocr tesseract-ocr-chi-sim"
    exit 1
fi

cd "$(dirname "$0")"
python3 -m pip install -r requirements.txt
python3 server.py --port 8081
```

---

### 任务 20：RAG Server 调用 PDF Parser

**文件**: `src/rag/server/server.cpp` 新增方法

```cpp
bool RAGServer::call_pdf_parser(const std::string& pdf_path,
                                 MultimodalDocument& result) {
    // HTTP POST 到 localhost:8081/parse
    std::string body = "{\"path\":\"" + json_escape(pdf_path) + "\"}";
    // ... send HTTP request to 8081, parse JSON response ...
}
```

**配置**:
```yaml
# rag_config.yaml
pdf_parser:
  enabled: true
  url: "http://localhost:8081"
  timeout_ms: 300000      # 5 分钟超时（大 PDF）
  ocr_enabled: true
  table_enabled: true
```

---

### 任务 21：端到端测试

**测试用例**：

| 测试 | 步骤 | 预期 |
|------|------|------|
| 文件上传 | 上传包含子目录的 zip | 目录结构保持，文件递归解析 |
| PDF 解析 | 上传含扫描页+双栏的 PDF | 扫描页 OCR，双栏顺序正确 |
| 表格提取 | 上传含跨页表格的 PDF | 跨页表格合并正确 |
| 重建索引 | 上传文档后点击重建 | 文档可检索 |
| kbase 检索 | 配置 use_kbase=true | 检索走 kbase_search |
| 删除文档 | 删除后文档不可检索 | 索引同步删除 |

---

## 任务汇总

| Phase | # | 任务 | 文件变更 |
|-------|---|------|----------|
| 1 | T1 | 后端文件上传接口 | `server.cpp` |
| 1 | T2 | 后端目录管理接口 | `server.cpp` |
| 1 | T3 | 后端文档列表增强 | `server.cpp` |
| 1 | T4 | 后端删除文档 | `server.cpp` |
| 1 | T5 | 前端文件管理面板 | `FileManager.tsx` |
| 1 | T6 | 前端上传 UI | `UploadDialog.tsx` |
| 1 | T7 | 前端目录管理 UI | `DirManager.tsx` |
| 2 | T8 | Python PDF 解析服务 | `pdf_parser/server.py` 等 |
| 2 | T9 | OCR 集成 | `pdf_parser/parser/ocr.py` |
| 2 | T10 | 表格提取 | `pdf_parser/parser/tables.py` |
| 2 | T11 | 版面分析 | `pdf_parser/parser/layout.py` |
| 2 | T12 | 后处理 | `pdf_parser/parser/postprocess.py` |
| 3 | T13 | 后端重建索引接口 | `server.cpp` |
| 3 | T14 | 增量索引支持 | `server.cpp` |
| 3 | T15 | 前端重建索引 UI | `RebuildDialog.tsx` |
| 4 | T16 | KbaseRetriever | `kbase_retriever.h/cpp` |
| 4 | T17 | KbaseBM25Retriever | `kbase_bm25_retriever.h/cpp` |
| 4 | T18 | 检索器工厂更新 | `retriever.cpp` |
| 4b | T18b | DB 适配器接口 | `db_adapter.h` |
| 4b | T18c | 适配器工厂 | `db_adapter_factory.cpp` |
| 4b | T18d | 迁移接口（导入/导出） | `server.cpp` |
| 4b | T18e | 迁移工具 CLI | `tools/migrate.cpp` |
| 5 | T19 | PDF 解析服务注册 | `requirements.txt`, `run.sh` |
| 5 | T20 | RAG Server 调用 PDF Parser | `server.cpp` |
| 5 | T21 | 端到端测试 | 新建测试文件 |

**共计：25 个任务**

---

## Phase 依赖图

```
Phase 1 (上传/目录)
    │
    ├── T1-T4: 后端上传/目录/删除接口
    └── T5-T7: 前端文件管理 UI
          │
          ▼
Phase 2 (PDF解析)  ── T8-T12 ──┐
    │                            │
    │  (T8-T12 可独立先行)         │
    ▼                            │
Phase 3 (重建索引)  T13-T15 ─────┤
    │                            │
    │                            ▼
    │                     Phase 5 (集成验证)
    │                        T19-T21
    │
    ▼
Phase 4 (kbase接入) T16-T18
    │
    ▼
Phase 4b (迁移框架) T18b-T18e
```
