# RAG 存储架构与 Web 功能扩展方案

> 日期：2026-09-03
> 目标：RAG 系统 (D:\code\book\engineering\rag)

---

## 一、当前 RAG 存储架构

### 1.1 存储布局

```
./rag_data/                          # 数据根目录 (可配置)
├── doc1.md
├── doc2.txt
├── book.pdf
└── index/                           # 索引目录 (可配置)
    ├── hnsw.bin                     # HNSW 向量索引 (二进制)
    └── bm25.bin                     # BM25 全文索引 (二进制)
```

### 1.2 各组件说明

| 组件 | 实现 | 存储位置 | 说明 |
|------|------|----------|------|
| **原始文档** | 文件系统 | `./rag_data/` | .md/.txt/.pdf 等文件 |
| **向量索引** | hnswlib | `./rag_data/index/hnsw.bin` | HNSW 图结构，内存映射持久化 |
| **全文索引** | 自实现 C++ BM25 | `./rag_data/index/bm25.bin` | 词频统计 + IDF，非 Elasticsearch |
| **文档元数据** | 内存 `unordered_map` | 服务进程内存 | doc_id → {file_path, title, chunk_ids} |
| **Embedding** | Ollama API (MiniLM) | 调用时实时生成 | 不持久化，重建索引时重新生成 |

### 1.3 为什么不用向量数据库？

| 选择 | 原因 |
|------|------|
| **不用 Milvus/Pinecone 等** | RTX 4060 Laptop 单机部署，8GB VRAM，16GB RAM，外置向量库开销大 |
| **用 hnswlib** | 轻量级，单机向量索引首选，内存映射持久化，召回率高 |
| **BM25 自实现** | C++ 内嵌，无外部依赖，启动快，数据量小场景足够 |

**结论**：当前架构适合单机小规模（万级文档）场景。如数据量增至百万级，可将 HNSW/BM25 替换为自研 kbase。

---

## 二、kbase 数据库接入方案

### 2.1 kbase 现有能力

| 模块 | 文件 | 能力 |
|------|------|------|
| 索引构建 | `kbase_index.c` | 扫描笔记目录，生成 embedding，构建 HNSW |
| 语义搜索 | `kbase_search.c` | `kbase_search()` → embedding + faiss_hnsw_index_search |
| 模型推理 | `infra/model_gguf.c` | MiniLM-L6 等 embedding 模型本地推理 |

**缺失能力**：
- BM25 全文检索（kbase 目前只有向量搜索）
- 向量索引的持久化加载

### 2.2 接入架构

```
当前:
  HybridRetriever ──→ HNSWRetriever (hnswlib)
                  └──→ BM25Retriever (自实现 in-memory)

接入后:
  HybridRetriever ──→ KbaseRetriever (wrap kbase_search + faiss_hnsw)
                  └──→ KbaseBM25Retriever (新增: 包装 kbase 全文索引)

如 kbase 无 BM25，可保留 RAG 侧 BM25Retriever，仅替换向量检索:
  HybridRetriever ──→ KbaseRetriever (kbase HNSW)
                  └──→ BM25Retriever (RAG 侧自实现)
```

### 2.3 新增文件

```
include/rag/
└── kbase_retriever.h        # KbaseRetriever 接口 + 工厂函数

src/rag/
└── kbase_retriever.cpp      # 包装 kbase_search()
```

### 2.4 KbaseRetriever 实现要点

```cpp
// 包装 kbase_search()，适配 Retriever 接口
class KbaseRetriever : public Retriever {
public:
    KbaseRetriever(kbase_index_t* idx, infra_model_t* model,
                   const RetrievalConfig& config);

    std::vector<RetrievalResult> retrieve(
        const std::string& query, int top_k) override;

    std::string name() const override { return "kbase"; }

private:
    kbase_index_t* idx_;
    infra_model_t* model_;
    RetrievalConfig config_;
};

// 返回映射:
//   kbase_result_t.note->content → RetrievalResult.content
//   kbase_result_t.note->path   → RetrievalResult.file_path
//   kbase_result_t.score        → RetrievalResult.vector_score
```

### 2.5 KbaseBM25Retriever 新增（如 kbase 扩展全文检索）

如 kbase 后续实现 BM25，在 kbase_index 中新增接口：

```c
// kbase_index.h 新增
kbase_bm25_result_t* kbase_bm25_search(
    kbase_index_t* idx, const char* query, int top_k, int* num_results);
```

包装为 `KbaseBM25Retriever` 替换 RAG 侧 BM25Retriever。

### 2.6 检索器工厂修改

```cpp
// src/rag/retriever.cpp
std::shared_ptr<Retriever> create_retriever(
    const RetrievalConfig& config,
    std::shared_ptr<VectorIndex> vector_index,
    std::shared_ptr<BM25Index> bm25_index,
    std::shared_ptr<EmbeddingService> embed_service)
{
    if (config.use_kbase) {
        // 使用 kbase
        auto kbase_idx = kbase_index_load(...);
        auto model = infra_model_load(...);
        return std::make_shared<KbaseRetriever>(kbase_idx, model, config);
    }
    // 回退到原有 hnswlib + BM25
    ...
}
```

---

## 三、Web 功能扩展

### 3.1 重建索引功能

#### 3.1.1 后端接口

```
POST /api/v1/index/rebuild
Request:  (无 body，或 { "force": true })
Response: { "status": "started", "job_id": "xxx" }

GET  /api/v1/index/rebuild/status
Response: { "status": "running|completed|failed", "progress": 0.75,
            "message": "Indexing doc 123 of 456" }
```

#### 3.1.2 实现要点

- 扫描 `./rag_data/` 下所有支持的文件
- 对每个文件：解析 → 分块 → 生成 embedding → 写入向量索引 + BM25
- 进度通过 `job_id` 轮询，**不阻塞 HTTP 请求**
- 重建前备份旧 `hnsw.bin` / `bm25.bin`

#### 3.1.3 前端 UI

- 设置面板中新增 **"重建索引"** 按钮
- 点击后显示进度条：`正在重建索引 (75%) - doc123.md`
- 完成后 toast 提示

---

### 3.2 文件上传功能

#### 3.2.1 后端接口

```
POST /api/v1/documents/upload
Content-Type: multipart/form-data
Fields:
  - file: 文件或文件夹 (zip/tar.gz 支持批量)
  - target_dir: 目标目录路径 (可选，默认 ./rag_data/)

Response: {
  "uploaded": 5,
  "skipped": 1,
  "errors": [],
  "files": [
    { "name": "book.pdf", "size": 2048000, "status": "uploaded" }
  ]
}

GET  /api/v1/documents
GET  /api/v1/documents/{id}
DELETE /api/v1/documents/{id}
```

#### 3.2.2 目录管理接口

```
POST   /api/v1/documents/dirs
       Body: { "path": "/my-books/algorithms" }
       Response: { "created": true, "path": "/my-books/algorithms" }

DELETE /api/v1/documents/dirs?path=/my-books/algorithms

GET    /api/v1/documents/dirs
       Response: { "dirs": ["/my-books", "/my-books/algorithms", ...] }
```

#### 3.2.3 递归解析策略

```
上传目录结构:
  /my-books/
  ├── algorithms/
  │   ├── introduction.md
  │   └── chapter1/
  │       └── sorting.pdf
  └── databases/
      └── sql-guide.txt

解析行为:
  1. 递归扫描所有子目录
  2. 按文件类型分发解析器:
     - .md/.txt  → MarkdownParser / TextParser
     - .pdf      → PDFParser (见第四章)
  3. 文件存入 ./rag_data/{对应目录结构}/
  4. 解析后自动加入索引（可选，增量或批量）
```

#### 3.2.4 前端 UI

```
┌─────────────────────────────────────────┐
│  📁 文件管理                              │
├─────────────────────────────────────────┤
│  [+ 新建文件夹]  [⬆ 上传文件/文件夹]       │
├─────────────────────────────────────────┤
│  📂 /my-books/                          │
│    📂 algorithms/                        │
│      📄 introduction.md        12KB     │
│      📄 chapter1/                       │
│        📄 sorting.pdf         2.3MB     │
│    📂 databases/                        │
│      📄 sql-guide.txt          45KB     │
├─────────────────────────────────────────┤
│  [重建索引]              当前索引: 123 个文档 │
└─────────────────────────────────────────┘
```

---

## 四、PDF 专业解析方案

### 4.1 设计目标

计算机领域书籍 PDF 的高质量解析，输出结构化文本块供 RAG 检索。

### 4.2 PDF 解析 Pipeline

```
┌─────────────────────────────────────────────────────────┐
│  第0层: 文件预处理                                         │
│  ├── 检测文件是否已加密，尝试解密                           │
│  ├── 检测 PDF 版本兼容性                                   │
│  └── 提取元数据 (title, author, page count)               │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│  第1层: 页面分类                                          │
│  每个页面分为以下类型之一:                                 │
│  ├── 扫描页 (scanned)    — 无可提取文字，需 OCR           │
│  ├── 混合页 (mixed)       — 有文字 + 有图片/扫描区域       │
│  └── 数字页 (digital)     — 纯文字，可直接提取              │
│                                                          │
│  分类依据:                                               │
│  - 文字提取成功率 < 阈值 → 标记为扫描页候选                │
│  - 图片面积占比 > 阈值 → 标记为混合页                      │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│  第2层: 版面分析 (Layout Analysis)                        │
│                                                          │
│  区域检测 (Bounding Box):                                 │
│  ├── 文本区域 (paragraph, heading, list_item, code)       │
│  ├── 表格区域 (table)                                     │
│  ├── 图片区域 (figure, photo, diagram)                   │
│  ├── 公式区域 (equation, inline_math, display_math)       │
│  └── 注释区域 (footnote, margin_note, watermark)          │
│                                                          │
│  版面模式识别:                                           │
│  ├── 单栏 (single_column)                                │
│  ├── 双栏 (two_column) — 学术论文常见                      │
│  ├── 双栏+边注 (two_column_with_margin) — 书籍常见         │
│  └── 混合 (mixed)                                         │
│                                                          │
│  工具: pdfminer.six (区域检测), PyMuPDF (坐标提取)        │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│  第3层: 内容提取                                          │
│                                                          │
│  3.1 文本提取                                            │
│  ├── pdfminer.six: 精确到字符位置、字体信息               │
│  ├── 字体分析: 等宽 → 代码块, 斜体 → 强调                 │
│  └── 阅读顺序: 按 (y, x) 分组还原双栏顺序                  │
│                                                          │
│  3.2 OCR 提取 (扫描页)                                   │
│  ├── Tesseract 5.x (本地, 支持中文+英文)                 │
│  ├── PDF 页 → 图像 (300 DPI, 灰度) → Tesseract           │
│  ├── 布局保持 OCR (--psm 6) 保持段落结构                  │
│  └── 方向检测 + 自动旋转校正                              │
│                                                          │
│  3.3 表格提取                                            │
│  ├── pdfplumber: 检测表格边框线                          │
│  ├── camelot: 支持无边框表格 (线检测)                     │
│  ├── tabula-py: Java 后端，精度高                        │
│  └── 输出: CSV/JSON 格式的二维表格结构                     │
│                                                          │
│  3.4 跨页表格处理                                        │
│  ├── 检测被 page break 打断的表格                        │
│  ├── 识别表头 (首行重复模式)                              │
│  ├── 合并相邻页的表格行                                    │
│  └── 验证列数一致性，剔除不匹配行                          │
│                                                          │
│  3.5 图表处理                                            │
│  ├── 图表区域裁剪保存为图片                                │
│  ├── BLIP/VLM 生成图表描述 (alt-text)                    │
│  └── 图表标题 + 图表内容 → 组合文本块                      │
│                                                          │
│  3.6 公式处理                                            │
│  ├── Mathpix OCR API 或 mathpix-markdown-it              │
│  ├── 检测 display_math: LaTeX 公式作为独立块              │
│  └── 检测 inline_math: 还原为文本描述                     │
│                                                          │
│  3.7 双栏排版处理                                        │
│  ├── 识别栏间隙 (空白列)                                  │
│  ├── 按栏分组 → 栏内按 y 排序 → 栏间按 x合并             │
│  ├── 边注区域识别 (x 坐标极端) → 降级处理                 │
│  └── 脚注追踪 (页底 footnote 与正文引用关联)              │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│  第4层: 后处理                                           │
│                                                          │
│  4.1 跨页段落合并                                        │
│  ├── 相邻页同段落 (最后一行不完整) → 合并                │
│  ├── 短行末尾 (word 不完整) → 标记待合并                 │
│  └── 与下页首行合并                                       │
│                                                          │
│  4.2 章节层级识别                                        │
│  ├── 目录提取: 解析 PDF 目录 (TOC) 获取章节结构          │
│  ├── 标题检测: # 标题 → 字体大小/粗体/颜色             │
│  ├── 层级推断: h1 > h2 > h3                              │
│  └── 建立章节 → 页面映射                                  │
│                                                          │
│  4.3 代码块结构化                                        │
│  ├── 等宽字体区域 → 预格式化代码块                        │
│  ├── 语法高亮 (可选: tree-sitter)                        │
│  └── 行号还原                                             │
│                                                          │
│  4.4 代码块切分 (长函数/长段落)                          │
│  ├── 超过 N 字符 → 按逻辑单元切分                         │
│  ├── 代码: 按空行/函数边界切分                           │
│  └── 文本: 按句子/段落边界切分                            │
│                                                          │
│  4.5 脚注/边注/水印处理                                   │
│  ├── 脚注区域标记，生成独立块（可选包含）                  │
│  ├── 水印区域检测，剔除出正文                             │
│  └── 页眉/页脚: 按固定位置区域剔除                         │
│                                                          │
│  4.6 特殊纸张方向                                        │
│  ├── 检测横向页面 (width > height)                        │
│  └── 旋转校正后按正常流程处理                              │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│  第5层: 输出                                              │
│                                                          │
│  输出格式 (MultimodalDocument):                          │
│  {                                                       │
│    "source_path": "book.pdf",                           │
│    "page_count": 456,                                   │
│    "text": "...",                                       │
│    "chunks": [                                          │
│      {                                                  │
│        "id": "uuid",                                    │
│        "content": "第3章 排序算法...",                   │
│        "source": { "file": "book.pdf",                  │
│                    "page_start": 45,                    │
│                    "page_end": 47,                      │
│                    "type": "text" | "table" | "code"   │
│                    "heading": "第3章 排序算法"           │
│                  }                                      │
│      }                                                  │
│    ],                                                   │
│    "images": [                                          │
│      { "path": "chunk_123.png",                         │
│        "extracted_text": "图表描述..." }                │
│    ],                                                   │
│    "tables": [                                          │
│      { "csv": "col1,col2\n...",                        │
│        "page": 123 }                                    │
│    ],                                                   │
│    "metadata": {                                        │
│      "title": "算法导论",                                │
│      "author": "Thomas H. Cormen",                     │
│      "toc": [{ "title": "...", "page": 45 }, ...]       │
│    }                                                    │
│  }                                                      │
└─────────────────────────────────────────────────────────┘
```

### 4.3 技术选型

| 功能 | 工具 | 语言 | 说明 |
|------|------|------|------|
| PDF 解析 (文本/坐标) | **pdfminer.six** | Python | 精确到字符级，适合复杂版面 |
| PDF 操作 (元数据/页面) | **PyMuPDF (fitz)** | Python | 高性能，跨页处理 |
| OCR | **Tesseract 5.x** | C | 本地运行，支持中文 |
| 表格提取 (有边框) | **pdfplumber** | Python | 精确边框检测 |
| 表格提取 (无边框) | **camelot** | Python | 表格线检测 |
| 图表描述 | **BLIP** / **LLaVA** | Python | VLM 本地推理 |
| 公式 OCR | **Mathpix API** 或 **OmniParse** | API/Python | LaTeX 还原 |
| Python 调用桥接 | **pybind11** | C++/Python | RAG 是 C++，PDF 解析用 Python 扩展 |

### 4.4 增量解析策略

```
上传新 PDF → 增量索引:
1. 检查 PDF 的文件 hash (SHA256)
2. 如已索引且 hash 未变 → 跳过
3. 如 hash 变化 → 全量重新解析
4. 新文件 → 增量加入向量索引 + BM25
```

### 4.5 尚未覆盖的边缘情况（设计留白）

以下问题当前方案未完全解决，留作后续优化：

| 边缘情况 | 当前处理 | 后续方向 |
|----------|----------|----------|
| 手写扫描件 | Tesseract 可能效果差 | 专业扫描书籍用专业 OCR 服务 |
| 矢量图形内嵌文字 | 目前无法提取 | Inkscape / 结构化提取 |
| 多语言混排 (中日英) | Tesseract 需要多语言包 | 混合识别后拼接 |
| 超大 PDF ( > 5000 页) | 流式解析 | 分批处理 + checkpoint |
| 扫描质量差 (噪点/歪斜) | 基础去噪 | 预处理管道增强 |
| 加密 PDF (密码保护) | 跳过 + 报错 | 提示用户解密后上传 |

---

## 五、实现优先级

| 优先级 | 任务 | 说明 |
|--------|------|------|
| **P0** | Web 文件上传 + 目录管理 | 用户最直接需要的功能 |
| **P1** | PDF 解析核心流程 | pdfminer + PyMuPDF + Tesseract |
| **P2** | 重建索引功能 | 上传后需要触发索引 |
| **P3** | kbase 向量检索接入 | 替换 hnswlib，发挥 kbase 已有能力 |
| **P4** | kbase BM25 全文检索 | 需 kbase 侧先实现 BM25 |
| **P5** | PDF 高级解析 | 跨页表格、公式 OCR、图表描述 |
| **P6** | 增量索引 | 避免全量重建 |

---

## 六、待确认事项

1. **kbase 侧 BM25**：kbase 目前只有向量检索，是否计划在 kbase 侧实现 BM25？
2. **PDF 解析部署**：PDF 解析依赖 Python + Tesseract，计划作为 C++ 扩展 (pybind11) 还是独立 Python 服务？
3. **上传文件大小限制**：是否有单文件/总上传量限制？
4. **文件目录结构持久化**：文档的目录层级信息是否需要存入索引（如用于按目录检索）？
