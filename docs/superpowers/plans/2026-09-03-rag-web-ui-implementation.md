# RAG Web UI 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建一个现代化的 RAG Web 交互提问界面，支持对话、文档引用、多轮对话上下文和文档预览，前端 React SPA + 后端 C++ REST API。

**Architecture:** React SPA (Vite 构建) 通过 HTTP/JSON 调用 C++ RAG 服务器。开发模式用 Vite 代理转发 `/api` 到 C++ 服务器；生产模式由 C++ 服务器直接托管 `web/dist` 静态文件。多轮上下文在前端拼接进 query 字符串（设计文档 3.2.4 方案），后端无需理解对话结构。

**Tech Stack:** React 18, TypeScript 5, Vite 5, Tailwind CSS 3.4, shadcn/ui 风格组件, React Query 5; 后端 C++17 (已有 server.cpp)

**设计文档:** `C:\Users\yinch\docs\superpowers\specs\2026-09-03-rag-web-ui-design.md`

## Global Constraints

- TypeScript 严格模式；组件化设计，单一职责
- 所有 API 调用必须有错误处理
- 响应式设计，支持移动端
- 后端仅 C++17 标准库 + Winsock，**不得引入新第三方依赖**（nlohmann_json 因 FetchContent 无法下载不可用）
- 后端验证方式：`g++ -fsyntax-only -std=c++17 -I include <文件>`（环境无法完整 CMake 构建）
- 后端 API 实际路径为 `/api/v1/*`（不是设计文档中的 `/api/*`），前端必须匹配
- 流式生成后端模块未实现（T22 已跳过），本计划 UI 使用加载动画 + 一次性完整响应；真正的逐字流式输出列为后续项

## 后端现状（关键事实，实现前必读）

`D:\code\book\engineering\rag\src\rag\server\server.cpp` 现状：

1. **路由**: `POST /api/v1/query`、`POST /api/v1/retrieve`、`GET /api/v1/documents`、`GET /api/v1/index/status`、`GET /health`、`GET /metrics`、`GET /`
2. **JSON 解析有 bug**: `handle_query` 中 `body.substr(start + 2, end - start - 3)` 会从值的起始引号一直截取到 body 末尾（长度计算为负数被 clamp），从未真正运行验证过
3. **JSON 输出无转义**: `chunk.content`、`doc.content` 直接拼进 JSON，内容含 `"` 或换行即产出非法 JSON
4. **无文档内容端点**: `handle_document(id)` 在 server.h 声明但未注册路由、未实现
5. **`Document` 结构含完整 `content` 字段**（`include/rag/types.h:37`），`Chunk` 含 `document_id`（types.h:66），内容端点可直接从 `engine_->list_documents()` 取内容，无需读磁盘
6. **`handle_connection` 在响应后又包了一层 HTTP 头**（server.cpp:304-311），而 `create_json_response` 已经生成完整 HTTP 响应 —— 实际发出的是"双层 HTTP 头"的畸形响应。这也必须修复

---

## Task 1: 后端 JSON 工具函数与响应修复

**Files:**
- Modify: `D:\code\book\engineering\rag\src\rag\server\server.cpp`

**Interfaces:**
- Consumes: 现有 server.cpp
- Produces: `json_escape()`, `extract_json_string()`, `extract_json_int()` 静态函数；修复后的 `handle_connection`（不再双层包头）；转义正确的 `handle_query`/`handle_retrieve`/`handle_documents` 响应；chunk JSON 新增 `document_id` 字段

- [ ] **Step 1: 在 `url_decode` 函数前（server.cpp 第 37 行 `// ========== HTTP 工具 ==========` 之后）添加 JSON 工具函数**

```cpp
// ========== JSON 工具（最小实现，仅满足本服务器需求） ==========

// JSON 字符串转义
static std::string json_escape(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 16);
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}

// 从 JSON body 中提取字符串字段（处理转义字符）
// 找到 "key" 后提取其字符串值；未找到返回 default_value
static std::string extract_json_string(const std::string& body, const std::string& key,
                                       const std::string& default_value = "") {
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return default_value;

    // 跳过 key、冒号和空白
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return default_value;
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) ++pos;

    // 值必须是字符串
    if (pos >= body.size() || body[pos] != '"') return default_value;
    ++pos;

    // 提取到未转义的结束引号
    std::string result;
    while (pos < body.size()) {
        char c = body[pos];
        if (c == '\\' && pos + 1 < body.size()) {
            char next = body[pos + 1];
            switch (next) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                default: result += next; break;
            }
            pos += 2;
        } else if (c == '"') {
            return result;  // 结束引号
        } else {
            result += c;
            ++pos;
        }
    }
    return result;  // 未闭合，返回已提取部分
}

// 从 JSON body 中提取整数字段
static int extract_json_int(const std::string& body, const std::string& key, int default_value) {
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return default_value;

    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return default_value;
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) ++pos;

    try {
        return std::stoi(body.substr(pos));
    } catch (...) {
        return default_value;
    }
}
```

同时在文件头部 `#include <regex>` 后添加：

```cpp
#include <cstdio>
```

- [ ] **Step 2: 修复 `handle_connection` 双层 HTTP 头问题（server.cpp:275-322）**

处理器返回的已是完整 HTTP 响应（`create_json_response` 生成），直接发送即可。将路由处理到发送响应之间的代码（276-311 行）替换为：

```cpp
        // 路由处理（处理器返回完整 HTTP 响应）
        std::string response;

        if (route == "/api/v1/query" && method == "POST") {
            response = handle_query(body);
        } else if (route == "/api/v1/retrieve" && method == "POST") {
            response = handle_retrieve(body);
        } else if (route == "/api/v1/documents" && method == "GET") {
            response = handle_documents();
        } else if (route.rfind("/api/v1/documents/", 0) == 0 && method == "GET") {
            // /api/v1/documents/{id}/content
            std::string rest = route.substr(std::string("/api/v1/documents/").size());
            if (rest.size() > 8 && rest.compare(rest.size() - 8, 8, "/content") == 0) {
                std::string doc_id = url_decode(rest.substr(0, rest.size() - 8));
                response = handle_document_content(doc_id);
            } else {
                response = handle_document(url_decode(rest));
            }
        } else if (route == "/api/v1/index/status" && method == "GET") {
            response = handle_index_status();
        } else if (route == "/health" && method == "GET") {
            response = handle_health();
        } else if (route == "/metrics" && method == "GET") {
            response = handle_metrics();
        } else if (method == "OPTIONS") {
            // CORS 预检
            response = "HTTP/1.1 204 No Content\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                       "Access-Control-Allow-Headers: Content-Type\r\n"
                       "Content-Length: 0\r\n\r\n";
        } else if (method == "GET") {
            // 静态文件服务（Task 3 实现 serve_static；未命中时返回 404）
            response = serve_static(route);
        } else {
            response = create_error_response("Not Found", 404);
        }

        // 发送响应
        send(client_socket, response.c_str(), response.size(), 0);
```

注意：原代码中 `status` 变量、`add_cors_headers` 调用、拼接 `http_response` 的部分全部删除（`add_cors_headers` 成员函数本身保留但不再被调用，CORS 头已在 `create_json_response` 内处理）。

- [ ] **Step 3: 在 server.h 中声明新处理器**

修改 `D:\code\book\engineering\rag\include\rag\server.h`，在 `handle_document` 声明后添加两个私有方法声明：

```cpp
    std::string handle_document(const std::string& id);
    std::string handle_document_content(const std::string& id);   // 新增：文档内容
    std::string serve_static(const std::string& route);            // 新增：静态文件
```

- [ ] **Step 4: 用健壮的解析和转义重写 `handle_query`（替换 server.cpp:350-400 整个函数）**

```cpp
std::string Server::handle_query(const std::string& body) {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    std::string query = extract_json_string(body, "query");
    int top_k = extract_json_int(body, "top_k", 5);

    if (query.empty()) {
        return create_error_response("Missing query parameter", 400);
    }

    auto result = engine_->query(query, top_k);

    std::ostringstream oss;
    oss << "{";
    oss << "\"answer\": \"" << json_escape(result.answer) << "\",";
    oss << "\"confidence\": " << result.confidence << ",";
    oss << "\"query_time_ms\": " << result.query_time_ms << ",";
    oss << "\"request_id\": \"" << json_escape(result.request_id) << "\",";
    oss << "\"chunks\": [";

    for (size_t i = 0; i < result.chunks.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& chunk = result.chunks[i];
        oss << "{";
        oss << "\"id\": \"" << json_escape(chunk.chunk.id) << "\",";
        oss << "\"document_id\": \"" << json_escape(chunk.chunk.document_id) << "\",";
        oss << "\"content\": \"" << json_escape(chunk.chunk.content) << "\",";
        oss << "\"file_path\": \"" << json_escape(chunk.chunk.metadata.file_path) << "\",";
        oss << "\"score\": " << chunk.score;
        oss << "}";
    }

    oss << "]}";
    return create_json_response(oss.str());
}
```

- [ ] **Step 5: 重写 `handle_retrieve` 的解析与转义（替换 server.cpp:402-434）**

```cpp
std::string Server::handle_retrieve(const std::string& body) {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    std::string query = extract_json_string(body, "query");
    int top_k = extract_json_int(body, "top_k", 5);

    if (query.empty()) {
        return create_error_response("Missing query parameter", 400);
    }

    auto results = engine_->retrieve(query, top_k);

    std::ostringstream oss;
    oss << "{\"results\": [";

    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& result = results[i];
        oss << "{";
        oss << "\"id\": \"" << json_escape(result.chunk.id) << "\",";
        oss << "\"document_id\": \"" << json_escape(result.chunk.document_id) << "\",";
        oss << "\"content\": \"" << json_escape(result.chunk.content) << "\",";
        oss << "\"file_path\": \"" << json_escape(result.chunk.metadata.file_path) << "\",";
        oss << "\"score\": " << result.score;
        oss << "}";
    }

    oss << "]}";
    return create_json_response(oss.str());
}
```

- [ ] **Step 6: 为 `handle_documents` 添加转义（替换 server.cpp:436-459）**

```cpp
std::string Server::handle_documents() {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    auto docs = engine_->list_documents();

    std::ostringstream oss;
    oss << "{\"documents\": [";

    for (size_t i = 0; i < docs.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& doc = docs[i];
        oss << "{";
        oss << "\"id\": \"" << json_escape(doc.id) << "\",";
        oss << "\"file_name\": \"" << json_escape(doc.metadata.file_name) << "\",";
        oss << "\"file_path\": \"" << json_escape(doc.metadata.file_path) << "\",";
        oss << "\"status\": " << static_cast<int>(doc.status);
        oss << "}";
    }

    oss << "], \"total\": " << docs.size() << "}";
    return create_json_response(oss.str());
}
```

- [ ] **Step 7: 语法验证**

先在 server.cpp 底部加两个桩（Task 2/3 再填充），以及 server.cpp 当前**缺少**的 `handle_document` 最小实现：

```cpp
std::string Server::handle_document(const std::string& id) {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }
    auto docs = engine_->list_documents();
    for (const auto& doc : docs) {
        if (doc.id == id) {
            std::ostringstream oss;
            oss << "{";
            oss << "\"id\": \"" << json_escape(doc.id) << "\",";
            oss << "\"file_name\": \"" << json_escape(doc.metadata.file_name) << "\",";
            oss << "\"file_path\": \"" << json_escape(doc.metadata.file_path) << "\",";
            oss << "\"status\": " << static_cast<int>(doc.status);
            oss << "}";
            return create_json_response(oss.str());
        }
    }
    return create_error_response("Document not found", 404);
}

// 桩：Task 2 填充
std::string Server::handle_document_content(const std::string& id) {
    (void)id;
    return create_error_response("Not implemented", 501);
}

// 桩：Task 3 填充
std::string Server::serve_static(const std::string& route) {
    (void)route;
    return create_error_response("Not Found", 404);
}
```

然后验证：

```bash
cd /d/code/book/engineering/rag
g++ -fsyntax-only -std=c++17 -I include src/rag/server/server.cpp
```

预期：无错误输出。

- [ ] **Step 8: Commit**

```bash
cd /d/code/book
git add engineering/rag/src/rag/server/server.cpp engineering/rag/include/rag/server.h
git commit -m "fix(rag/server): 修复JSON解析与转义，修复双层HTTP头，chunk响应增加document_id"
```

---

## Task 2: 后端文档内容端点

**Files:**
- Modify: `D:\code\book\engineering\rag\src\rag\server\server.cpp`

**Interfaces:**
- Consumes: Task 1 的 `json_escape`、`handle_document_content` 桩、`engine_->list_documents()`（返回 `std::vector<Document>`，`Document.content` / `Document.metadata.file_path` / `Document.metadata.title`）
- Produces: `GET /api/v1/documents/{id}/content` → `{"id","file_path","content","title"}`，前端 `useDocument.loadDocument` 依赖此格式

- [ ] **Step 1: 用真实实现替换 Task 1 的 `handle_document_content` 桩**

```cpp
std::string Server::handle_document_content(const std::string& id) {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    auto docs = engine_->list_documents();
    for (const auto& doc : docs) {
        // 按文档 ID 匹配；同时允许按 file_path 匹配（前端只有 chunk 的 file_path 时也能用）
        if (doc.id == id || doc.metadata.file_path == id) {
            std::ostringstream oss;
            oss << "{";
            oss << "\"id\": \"" << json_escape(doc.id) << "\",";
            oss << "\"file_path\": \"" << json_escape(doc.metadata.file_path) << "\",";
            oss << "\"title\": \"" << json_escape(doc.metadata.title.empty()
                                                 ? doc.metadata.file_name
                                                 : doc.metadata.title) << "\",";
            oss << "\"content\": \"" << json_escape(doc.content) << "\"";
            oss << "}";
            return create_json_response(oss.str());
        }
    }

    return create_error_response("Document not found", 404);
}
```

- [ ] **Step 2: 语法验证**

```bash
cd /d/code/book/engineering/rag
g++ -fsyntax-only -std=c++17 -I include src/rag/server/server.cpp
```

预期：无错误输出。

- [ ] **Step 3: Commit**

```bash
cd /d/code/book
git add engineering/rag/src/rag/server/server.cpp
git commit -m "feat(rag/server): 新增 GET /api/v1/documents/{id}/content 文档内容端点"
```

---

## Task 3: 后端静态文件服务（托管 web/dist）

**Files:**
- Modify: `D:\code\book\engineering\rag\src\rag\server\server.cpp`
- Modify: `D:\code\book\engineering\rag\include\rag\config.h`

**Interfaces:**
- Consumes: Task 1 的 `serve_static` 桩与路由
- Produces: `ServerConfig.static_dir`（默认空，空则不服务静态文件）；`serve_static(route)` 实现，含路径穿越防护与 MIME 映射

- [ ] **Step 1: 在 `ServerConfig` 中添加 `static_dir` 字段**

修改 `D:\code\book\engineering\rag\include\rag\config.h` 的 `ServerConfig`（第 131 行附近），在 `cors_origin` 字段后添加：

```cpp
    std::string static_dir;                     // 静态文件目录（web/dist），空则不启用
```

- [ ] **Step 2: 用真实实现替换 `serve_static` 桩**

在 server.cpp 头部 include 区添加 `#include <filesystem>`，然后替换桩：

```cpp
std::string Server::serve_static(const std::string& route) {
    if (config_.static_dir.empty()) {
        return create_error_response("Not Found", 404);
    }

    // 路径穿越防护：拒绝包含 .. 的路径
    std::string clean = route;
    if (clean.empty() || clean == "/") {
        clean = "/index.html";
    }
    if (clean.find("..") != std::string::npos) {
        return create_error_response("Forbidden", 403);
    }

    namespace fs = std::filesystem;
    fs::path file_path = fs::path(config_.static_dir) / clean.substr(1);

    // SPA 回退：文件不存在且非资源文件时返回 index.html
    std::error_code ec;
    if (!fs::exists(file_path, ec) || fs::is_directory(file_path, ec)) {
        if (clean.rfind("/assets/", 0) == 0) {
            return create_error_response("Not Found", 404);
        }
        file_path = fs::path(config_.static_dir) / "index.html";
        if (!fs::exists(file_path, ec)) {
            return create_error_response("Not Found", 404);
        }
    }

    // 读取文件（二进制模式，支持图片/字体）
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return create_error_response("Not Found", 404);
    }
    std::ostringstream content;
    content << file.rdbuf();
    std::string body = content.str();

    // MIME 类型映射
    std::string ext = file_path.extension().string();
    std::string mime = "application/octet-stream";
    if (ext == ".html") mime = "text/html; charset=utf-8";
    else if (ext == ".js")   mime = "application/javascript";
    else if (ext == ".css")  mime = "text/css";
    else if (ext == ".json") mime = "application/json";
    else if (ext == ".svg")  mime = "image/svg+xml";
    else if (ext == ".png")  mime = "image/png";
    else if (ext == ".jpg" || ext == ".jpeg") mime = "image/jpeg";
    else if (ext == ".ico")  mime = "image/x-icon";
    else if (ext == ".woff" || ext == ".woff2") mime = "font/woff2";

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: " << mime << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Cache-Control: no-cache\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}
```

- [ ] **Step 3: 确认 handle_root 无需改动**

`handle_connection` 路由中 `GET /` 已落入 `serve_static`（Task 1 Step 2 的 `else if (method == "GET")` 分支覆盖所有未匹配的 GET）。`handle_root` 保留不动（不再被路由到，留作参考）。无需改动。

- [ ] **Step 4: 语法验证**

```bash
cd /d/code/book/engineering/rag
g++ -fsyntax-only -std=c++17 -I include src/rag/server/server.cpp
```

预期：无错误输出。

- [ ] **Step 5: Commit**

```bash
cd /d/code/book
git add engineering/rag/src/rag/server/server.cpp engineering/rag/include/rag/config.h
git commit -m "feat(rag/server): 静态文件服务，支持托管 web/dist（SPA回退+MIME映射）"
```

---

## Task 4: 前端项目初始化

**Files:**
- Create: `D:\code\book\web\package.json`
- Create: `D:\code\book\web\vite.config.ts`
- Create: `D:\code\book\web\tsconfig.json`
- Create: `D:\code\book\web\tsconfig.node.json`
- Create: `D:\code\book\web\tailwind.config.js`
- Create: `D:\code\book\web\postcss.config.js`
- Create: `D:\code\book\web\index.html`
- Create: `D:\code\book\web\src\main.tsx`
- Create: `D:\code\book\web\src\index.css`
- Create: `D:\code\book\web\src\App.tsx`（占位版，Task 12 替换）

**Interfaces:**
- Consumes: Node.js 18+, npm；后端 `http://localhost:8080`
- Produces: 可运行的 Vite + React + Tailwind 项目，dev 代理 `/api`、`/health`、`/metrics` → 8080

- [ ] **Step 1: 创建目录结构**

```bash
mkdir -p /d/code/book/web/src/{components/ui,hooks,lib}
```

- [ ] **Step 2: 创建 package.json**

```json
{
  "name": "rag-web-ui",
  "private": true,
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "tsc && vite build",
    "preview": "vite preview"
  },
  "dependencies": {
    "react": "^18.2.0",
    "react-dom": "^18.2.0",
    "@tanstack/react-query": "^5.0.0",
    "react-markdown": "^9.0.0",
    "react-syntax-highlighter": "^15.5.0",
    "@types/react-syntax-highlighter": "^15.5.11",
    "@radix-ui/react-dialog": "^1.0.5",
    "@radix-ui/react-slider": "^1.1.2",
    "@radix-ui/react-switch": "^1.0.3",
    "lucide-react": "^0.300.0",
    "clsx": "^2.0.0",
    "tailwind-merge": "^2.0.0"
  },
  "devDependencies": {
    "@types/react": "^18.2.0",
    "@types/react-dom": "^18.2.0",
    "@vitejs/plugin-react": "^4.0.0",
    "typescript": "^5.3.0",
    "vite": "^5.0.0",
    "tailwindcss": "^3.4.0",
    "postcss": "^8.4.32",
    "autoprefixer": "^10.4.16"
  }
}
```

- [ ] **Step 3: 创建 vite.config.ts（代理匹配后端实际路径）**

```typescript
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
      '/health': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
      '/metrics': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
    },
  },
})
```

- [ ] **Step 4: 创建 tsconfig.json 和 tsconfig.node.json**

```json
{
  "compilerOptions": {
    "target": "ES2020",
    "useDefineForClassFields": true,
    "lib": ["ES2020", "DOM", "DOM.Iterable"],
    "module": "ESNext",
    "skipLibCheck": true,
    "moduleResolution": "bundler",
    "allowImportingTsExtensions": true,
    "resolveJsonModule": true,
    "isolatedModules": true,
    "noEmit": true,
    "jsx": "react-jsx",
    "strict": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "noFallthroughCasesInSwitch": true,
    "baseUrl": ".",
    "paths": {
      "@/*": ["./src/*"]
    }
  },
  "include": ["src"],
  "references": [{ "path": "./tsconfig.node.json" }]
}
```

```json
{
  "compilerOptions": {
    "composite": true,
    "skipLibCheck": true,
    "module": "ESNext",
    "moduleResolution": "bundler",
    "allowSyntheticDefaultImports": true
  },
  "include": ["vite.config.ts"]
}
```

- [ ] **Step 5: 创建 tailwind.config.js 和 postcss.config.js**

```javascript
/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  darkMode: 'class',
  theme: {
    extend: {},
  },
  plugins: [],
}
```

```javascript
export default {
  plugins: {
    tailwindcss: {},
    autoprefixer: {},
  },
}
```

- [ ] **Step 6: 创建 index.html**

```html
<!DOCTYPE html>
<html lang="zh-CN" class="h-full">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>D-code-book RAG</title>
  </head>
  <body class="h-full">
    <div id="root" class="h-full"></div>
    <script type="module" src="/src/main.tsx"></script>
  </body>
</html>
```

- [ ] **Step 7: 创建 src/main.tsx、src/index.css 和占位 src/App.tsx**

```typescript
import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App'
import './index.css'

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
)
```

```css
@tailwind base;
@tailwind components;
@tailwind utilities;

@layer base {
  html {
    @apply h-full;
  }
  body {
    @apply h-full bg-gray-50 text-gray-900 dark:bg-gray-900 dark:text-gray-100;
  }
}
```

```typescript
// 占位版 App（Task 12 替换为完整版）
export default function App() {
  return <div className="p-4">RAG UI 初始化完成</div>
}
```

- [ ] **Step 8: 安装依赖并验证启动**

```bash
cd /d/code/book/web
npm install
npm run dev
```

预期：Vite 启动在 http://localhost:5173，页面显示"RAG UI 初始化完成"。验证后 Ctrl+C 停止。

- [ ] **Step 9: Commit**

```bash
cd /d/code/book
git add web/
git commit -m "feat(web): 初始化 RAG Web UI 项目（Vite+React+Tailwind）"
```

---

## Task 5: 前端类型定义和工具函数

**Files:**
- Create: `D:\code\book\web\src\types.ts`
- Create: `D:\code\book\web\src\lib\utils.ts`

**Interfaces:**
- Consumes: 无
- Produces: `Message`, `ChunkReference`（含 `document_id`）, `QueryResponse`, `RetrieveResponse`, `QueryOptions`, `Settings`, `DocumentContent` 类型；`cn()`, `generateId()`, `formatLatency()` 工具函数

- [ ] **Step 1: 创建 src/types.ts**

```typescript
// 消息
export interface Message {
  id: string;
  role: 'user' | 'assistant';
  content: string;
  timestamp: number;
  chunks?: ChunkReference[];
  isStreaming?: boolean;
}

// 文档引用（document_id 用于加载完整文档预览）
export interface ChunkReference {
  id: string;
  document_id?: string;
  content: string;
  file_path: string;
  score: number;
}

// 查询响应（匹配后端 /api/v1/query 实际输出）
export interface QueryResponse {
  answer: string;
  chunks: ChunkReference[];
  confidence: number;
  query_time_ms: number;
  request_id?: string;
}

// 检索响应（匹配后端 /api/v1/retrieve 实际输出）
export interface RetrieveResponse {
  results: ChunkReference[];
}

// 查询选项（当前后端仅 topK 生效，其余持久化备用）
export interface QueryOptions {
  topK: number;
  temperature: number;
  maxTokens: number;
  useRerank: boolean;
}

// 设置（持久化到 localStorage）
export interface Settings {
  topK: number;
  minScore: number;
  temperature: number;
  maxTokens: number;
  useRerank: boolean;
  maxTurns: number;
}

// 文档内容（匹配后端 /api/v1/documents/{id}/content 输出）
export interface DocumentContent {
  id: string;
  file_path: string;
  content: string;
  title: string;
}
```

- [ ] **Step 2: 创建 src/lib/utils.ts**

```typescript
import { type ClassValue, clsx } from 'clsx'
import { twMerge } from 'tailwind-merge'

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}

// 生成唯一 ID
export function generateId(): string {
  return `${Date.now()}-${Math.random().toString(36).substr(2, 9)}`
}

// 格式化延迟
export function formatLatency(ms: number): string {
  if (ms < 1000) return `${ms}ms`
  return `${(ms / 1000).toFixed(2)}s`
}
```

- [ ] **Step 3: Commit**

```bash
cd /d/code/book
git add web/src/types.ts web/src/lib/utils.ts
git commit -m "feat(web): 类型定义与工具函数"
```

---

## Task 6: 前端 UI 基础组件

**Files:**
- Create: `D:\code\book\web\src\components\ui\button.tsx`
- Create: `D:\code\book\web\src\components\ui\scroll-area.tsx`
- Create: `D:\code\book\web\src\components\ui\card.tsx`
- Create: `D:\code\book\web\src\components\ui\dialog.tsx`
- Create: `D:\code\book\web\src\components\ui\slider.tsx`
- Create: `D:\code\book\web\src\components\ui\switch.tsx`

**Interfaces:**
- Consumes: `cn()` from `@/lib/utils`；Radix UI 原语（dialog/slider/switch，已在 package.json）
- Produces: `Button`, `ScrollArea`, `Card`/`CardHeader`/`CardTitle`/`CardContent`, `Dialog` 系列, `Slider`, `Switch` 组件

- [ ] **Step 1: 创建 button.tsx**

```typescript
import * as React from 'react'
import { cn } from '@/lib/utils'

export interface ButtonProps
  extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: 'default' | 'destructive' | 'outline' | 'secondary' | 'ghost' | 'link'
  size?: 'default' | 'sm' | 'lg' | 'icon'
}

const Button = React.forwardRef<HTMLButtonElement, ButtonProps>(
  ({ className, variant = 'default', size = 'default', ...props }, ref) => {
    return (
      <button
        className={cn(
          'inline-flex items-center justify-center whitespace-nowrap rounded-md text-sm font-medium transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-blue-500 focus-visible:ring-offset-2 disabled:pointer-events-none disabled:opacity-50',
          {
            'bg-blue-600 text-white hover:bg-blue-700': variant === 'default',
            'bg-red-600 text-white hover:bg-red-700': variant === 'destructive',
            'border border-gray-300 bg-transparent hover:bg-gray-100 dark:border-gray-600 dark:hover:bg-gray-800': variant === 'outline',
            'bg-gray-200 text-gray-900 hover:bg-gray-300 dark:bg-gray-700 dark:text-gray-100 dark:hover:bg-gray-600': variant === 'secondary',
            'hover:bg-gray-100 dark:hover:bg-gray-800': variant === 'ghost',
            'text-blue-600 underline-offset-4 hover:underline': variant === 'link',
          },
          {
            'h-10 px-4 py-2': size === 'default',
            'h-9 rounded-md px-3': size === 'sm',
            'h-11 rounded-md px-8': size === 'lg',
            'h-10 w-10': size === 'icon',
          },
          className
        )}
        ref={ref}
        {...props}
      />
    )
  }
)
Button.displayName = 'Button'

export { Button }
```

- [ ] **Step 2: 创建 scroll-area.tsx**

```typescript
import * as React from 'react'
import { cn } from '@/lib/utils'

export interface ScrollAreaProps extends React.HTMLAttributes<HTMLDivElement> {}

const ScrollArea = React.forwardRef<HTMLDivElement, ScrollAreaProps>(
  ({ className, children, ...props }, ref) => (
    <div
      ref={ref}
      className={cn('relative overflow-hidden', className)}
      {...props}
    >
      <div className="h-full w-full overflow-y-auto">
        {children}
      </div>
    </div>
  )
)
ScrollArea.displayName = 'ScrollArea'

export { ScrollArea }
```

- [ ] **Step 3: 创建 card.tsx**

```typescript
import * as React from 'react'
import { cn } from '@/lib/utils'

const Card = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
  ({ className, ...props }, ref) => (
    <div
      ref={ref}
      className={cn(
        'rounded-lg border border-gray-200 bg-white shadow-sm dark:border-gray-700 dark:bg-gray-800',
        className
      )}
      {...props}
    />
  )
)
Card.displayName = 'Card'

const CardHeader = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
  ({ className, ...props }, ref) => (
    <div ref={ref} className={cn('flex flex-col space-y-1.5 p-4', className)} {...props} />
  )
)
CardHeader.displayName = 'CardHeader'

const CardTitle = React.forwardRef<HTMLHeadingElement, React.HTMLAttributes<HTMLHeadingElement>>(
  ({ className, ...props }, ref) => (
    <h3 ref={ref} className={cn('text-lg font-semibold leading-none tracking-tight', className)} {...props} />
  )
)
CardTitle.displayName = 'CardTitle'

const CardContent = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
  ({ className, ...props }, ref) => (
    <div ref={ref} className={cn('p-4 pt-0', className)} {...props} />
  )
)
CardContent.displayName = 'CardContent'

export { Card, CardHeader, CardTitle, CardContent }
```

- [ ] **Step 4: 创建 dialog.tsx（基于 Radix UI）**

```typescript
import * as React from 'react'
import * as DialogPrimitive from '@radix-ui/react-dialog'
import { X } from 'lucide-react'
import { cn } from '@/lib/utils'

const Dialog = DialogPrimitive.Root
const DialogTrigger = DialogPrimitive.Trigger
const DialogPortal = DialogPrimitive.Portal
const DialogClose = DialogPrimitive.Close

const DialogOverlay = React.forwardRef<
  React.ElementRef<typeof DialogPrimitive.Overlay>,
  React.ComponentPropsWithoutRef<typeof DialogPrimitive.Overlay>
>(({ className, ...props }, ref) => (
  <DialogPrimitive.Overlay
    ref={ref}
    className={cn('fixed inset-0 z-50 bg-black/80', className)}
    {...props}
  />
))
DialogOverlay.displayName = DialogPrimitive.Overlay.displayName

const DialogContent = React.forwardRef<
  React.ElementRef<typeof DialogPrimitive.Content>,
  React.ComponentPropsWithoutRef<typeof DialogPrimitive.Content>
>(({ className, children, ...props }, ref) => (
  <DialogPortal>
    <DialogOverlay />
    <DialogPrimitive.Content
      ref={ref}
      className={cn(
        'fixed left-1/2 top-1/2 z-50 grid w-full max-w-lg -translate-x-1/2 -translate-y-1/2 gap-4 border border-gray-200 bg-white p-6 shadow-lg sm:rounded-lg dark:border-gray-700 dark:bg-gray-800',
        className
      )}
      {...props}
    >
      {children}
      <DialogPrimitive.Close className="absolute right-4 top-4 rounded-sm opacity-70 hover:opacity-100 focus:outline-none">
        <X className="h-4 w-4" />
        <span className="sr-only">关闭</span>
      </DialogPrimitive.Close>
    </DialogPrimitive.Content>
  </DialogPortal>
))
DialogContent.displayName = DialogPrimitive.Content.displayName

const DialogHeader = ({ className, ...props }: React.HTMLAttributes<HTMLDivElement>) => (
  <div className={cn('flex flex-col space-y-1.5 text-left', className)} {...props} />
)
DialogHeader.displayName = 'DialogHeader'

const DialogTitle = React.forwardRef<
  React.ElementRef<typeof DialogPrimitive.Title>,
  React.ComponentPropsWithoutRef<typeof DialogPrimitive.Title>
>(({ className, ...props }, ref) => (
  <DialogPrimitive.Title
    ref={ref}
    className={cn('text-lg font-semibold leading-none tracking-tight', className)}
    {...props}
  />
))
DialogTitle.displayName = DialogPrimitive.Title.displayName

export { Dialog, DialogPortal, DialogOverlay, DialogClose, DialogTrigger, DialogContent, DialogHeader, DialogTitle }
```

- [ ] **Step 5: 创建 slider.tsx**

```typescript
import * as React from 'react'
import * as SliderPrimitive from '@radix-ui/react-slider'
import { cn } from '@/lib/utils'

const Slider = React.forwardRef<
  React.ElementRef<typeof SliderPrimitive.Root>,
  React.ComponentPropsWithoutRef<typeof SliderPrimitive.Root>
>(({ className, ...props }, ref) => (
  <SliderPrimitive.Root
    ref={ref}
    className={cn('relative flex w-full touch-none select-none items-center', className)}
    {...props}
  >
    <SliderPrimitive.Track className="relative h-2 w-full grow overflow-hidden rounded-full bg-gray-200 dark:bg-gray-700">
      <SliderPrimitive.Range className="absolute h-full bg-blue-600" />
    </SliderPrimitive.Track>
    <SliderPrimitive.Thumb className="block h-5 w-5 rounded-full border-2 border-blue-600 bg-white transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-blue-500 disabled:pointer-events-none disabled:opacity-50" />
  </SliderPrimitive.Root>
))
Slider.displayName = SliderPrimitive.Root.displayName

export { Slider }
```

- [ ] **Step 6: 创建 switch.tsx**

```typescript
import * as React from 'react'
import * as SwitchPrimitives from '@radix-ui/react-switch'
import { cn } from '@/lib/utils'

const Switch = React.forwardRef<
  React.ElementRef<typeof SwitchPrimitives.Root>,
  React.ComponentPropsWithoutRef<typeof SwitchPrimitives.Root>
>(({ className, ...props }, ref) => (
  <SwitchPrimitives.Root
    className={cn(
      'peer inline-flex h-6 w-11 shrink-0 cursor-pointer items-center rounded-full border-2 border-transparent transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-blue-500 disabled:cursor-not-allowed disabled:opacity-50 data-[state=checked]:bg-blue-600 data-[state=unchecked]:bg-gray-200 dark:data-[state=unchecked]:bg-gray-700',
      className
    )}
    {...props}
    ref={ref}
  >
    <SwitchPrimitives.Thumb
      className={cn(
        'pointer-events-none block h-5 w-5 rounded-full bg-white shadow-lg ring-0 transition-transform data-[state=checked]:translate-x-5 data-[state=unchecked]:translate-x-0'
      )}
    />
  </SwitchPrimitives.Root>
))
Switch.displayName = SwitchPrimitives.Root.displayName

export { Switch }
```

- [ ] **Step 7: Commit**

```bash
cd /d/code/book
git add web/src/components/ui/
git commit -m "feat(web): UI 基础组件（Button/ScrollArea/Card/Dialog/Slider/Switch）"
```

---

## Task 7: 前端 API Hooks

**Files:**
- Create: `D:\code\book\web\src\hooks\useRAG.ts`
- Create: `D:\code\book\web\src\hooks\useDocument.ts`

**Interfaces:**
- Consumes: 后端 `POST /api/v1/query`（body: `{query, top_k}`，响应: QueryResponse）、`GET /api/v1/documents/{id}/content`（响应: DocumentContent）、`GET /health`
- Produces: `useRAG() → { query, healthCheck }`；`useDocument() → { previewDocument, isLoading, error, loadDocument, closePreview }`

注意：后端是 POST `/api/v1/retrieve` 且前端 UI 未使用纯检索，故不实现 retrieve。多轮上下文由 Task 11 在前端拼接进 query 字符串，**不发送 context 数组**（后端不解析该字段）。

- [ ] **Step 1: 创建 src/hooks/useRAG.ts**

```typescript
import { useCallback } from 'react'
import type { QueryOptions, QueryResponse } from '@/types'

export const useRAG = () => {
  const query = useCallback(async (
    queryText: string,
    options: QueryOptions
  ): Promise<QueryResponse> => {
    const response = await fetch('/api/v1/query', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        query: queryText,
        top_k: options.topK
      })
    })

    if (!response.ok) {
      throw new Error(`查询失败: ${response.status} ${response.statusText}`)
    }

    return response.json()
  }, [])

  const healthCheck = useCallback(async (): Promise<{ status: string }> => {
    const response = await fetch('/health')
    if (!response.ok) {
      throw new Error(`健康检查失败: ${response.statusText}`)
    }
    return response.json()
  }, [])

  return { query, healthCheck }
}
```

- [ ] **Step 2: 创建 src/hooks/useDocument.ts**

```typescript
import { useState, useCallback } from 'react'
import type { DocumentContent } from '@/types'

export const useDocument = () => {
  const [previewDocument, setPreviewDocument] = useState<DocumentContent | null>(null)
  const [isLoading, setIsLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)

  // id 可以是 document_id 或 file_path（后端两种都支持匹配）
  const loadDocument = useCallback(async (id: string) => {
    setIsLoading(true)
    setError(null)

    try {
      const response = await fetch(`/api/v1/documents/${encodeURIComponent(id)}/content`)
      if (!response.ok) {
        throw new Error(`加载文档失败: ${response.status} ${response.statusText}`)
      }
      const doc: DocumentContent = await response.json()
      setPreviewDocument(doc)
    } catch (err) {
      setError(err instanceof Error ? err.message : '加载文档失败')
    } finally {
      setIsLoading(false)
    }
  }, [])

  const closePreview = useCallback(() => {
    setPreviewDocument(null)
    setError(null)
  }, [])

  return {
    previewDocument,
    isLoading,
    error,
    loadDocument,
    closePreview
  }
}
```

- [ ] **Step 3: Commit**

```bash
cd /d/code/book
git add web/src/hooks/useRAG.ts web/src/hooks/useDocument.ts
git commit -m "feat(web): RAG 与文档 API hooks（匹配后端 /api/v1 实际接口）"
```

---

## Task 8: 聊天组件与历史持久化

**Files:**
- Create: `D:\code\book\web\src\hooks\useChat.ts`
- Create: `D:\code\book\web\src\components\MessageItem.tsx`
- Create: `D:\code\book\web\src\components\MessageList.tsx`
- Create: `D:\code\book\web\src\components\InputBox.tsx`
- Create: `D:\code\book\web\src\components\Chat.tsx`

**Interfaces:**
- Consumes: `useRAG().query`, `Message`, `ChunkReference`, `QueryOptions`, `generateId`, UI 组件
- Produces: `useChat() → { messages, isLoading, error, addMessage, clearMessages, setIsLoading, setError }`（localStorage 持久化，key: `rag-chat-history`）；`Chat` 组件 props: `{ options: QueryOptions, buildQuery?: (content: string) => string, onChunkClick?: (chunk: ChunkReference) => void, onAssistantMessage?: (content: string) => void }`

- [ ] **Step 1: 创建 src/hooks/useChat.ts（含 localStorage 持久化）**

```typescript
import { useState, useCallback, useEffect } from 'react'
import type { Message } from '@/types'
import { generateId } from '@/lib/utils'

const STORAGE_KEY = 'rag-chat-history'
const MAX_PERSISTED = 100  // 最多持久化 100 条消息

export const useChat = () => {
  const [messages, setMessages] = useState<Message[]>(() => {
    try {
      const saved = localStorage.getItem(STORAGE_KEY)
      return saved ? JSON.parse(saved) : []
    } catch {
      return []
    }
  })
  const [isLoading, setIsLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)

  // 持久化到 localStorage
  useEffect(() => {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(messages.slice(-MAX_PERSISTED)))
    } catch {
      // 存储满时静默失败
    }
  }, [messages])

  const addMessage = useCallback((message: Omit<Message, 'id' | 'timestamp'>) => {
    const newMessage: Message = {
      ...message,
      id: generateId(),
      timestamp: Date.now()
    }
    setMessages(prev => [...prev, newMessage])
    return newMessage
  }, [])

  const clearMessages = useCallback(() => {
    setMessages([])
    setError(null)
    localStorage.removeItem(STORAGE_KEY)
  }, [])

  return {
    messages,
    isLoading,
    error,
    addMessage,
    clearMessages,
    setIsLoading,
    setError
  }
}
```

- [ ] **Step 2: 创建 src/components/MessageItem.tsx**

```typescript
import { useState } from 'react'
import ReactMarkdown from 'react-markdown'
import { Prism as SyntaxHighlighter } from 'react-syntax-highlighter'
import { vscDarkPlus } from 'react-syntax-highlighter/dist/esm/styles/prism'
import type { Message, ChunkReference } from '@/types'
import { cn } from '@/lib/utils'
import { FileText, ChevronDown, ChevronUp, Copy, Check } from 'lucide-react'

interface MessageItemProps {
  message: Message
  onChunkClick?: (chunk: ChunkReference) => void
}

export const MessageItem = ({ message, onChunkClick }: MessageItemProps) => {
  const [expandedChunks, setExpandedChunks] = useState(false)
  const [copied, setCopied] = useState(false)

  const isUser = message.role === 'user'

  const handleCopy = async () => {
    await navigator.clipboard.writeText(message.content)
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }

  return (
    <div className={cn('flex gap-3 p-4', isUser ? 'justify-end' : 'justify-start')}>
      <div className={cn(
        'max-w-[85%] md:max-w-[75%] rounded-lg px-4 py-2',
        isUser ? 'bg-blue-600 text-white' : 'bg-gray-100 dark:bg-gray-800'
      )}>
        <div className={cn(
          'text-xs mb-1',
          isUser ? 'text-blue-200' : 'text-gray-500 dark:text-gray-400'
        )}>
          {isUser ? '用户' : 'AI 助手'}
        </div>

        <div className="prose prose-sm dark:prose-invert max-w-none break-words">
          <ReactMarkdown
            components={{
              code({ className, children, ...props }) {
                const match = /language-(\w+)/.exec(className || '')
                return match ? (
                  <SyntaxHighlighter
                    style={vscDarkPlus}
                    language={match[1]}
                    PreTag="div"
                  >
                    {String(children).replace(/\n$/, '')}
                  </SyntaxHighlighter>
                ) : (
                  <code className={className} {...props}>
                    {children}
                  </code>
                )
              }
            }}
          >
            {message.content}
          </ReactMarkdown>
        </div>

        {message.chunks && message.chunks.length > 0 && (
          <div className="mt-3 pt-3 border-t border-gray-200 dark:border-gray-700">
            <button
              onClick={() => setExpandedChunks(!expandedChunks)}
              className="flex items-center gap-1 text-sm text-gray-600 hover:text-gray-900 dark:text-gray-400 dark:hover:text-gray-100"
            >
              <FileText className="w-4 h-4" />
              引用文档 ({message.chunks.length})
              {expandedChunks ? <ChevronUp className="w-4 h-4" /> : <ChevronDown className="w-4 h-4" />}
            </button>

            {expandedChunks && (
              <div className="mt-2 space-y-2">
                {message.chunks.map((chunk) => (
                  <button
                    key={chunk.id}
                    onClick={() => onChunkClick?.(chunk)}
                    className="w-full text-left p-2 rounded bg-gray-50 hover:bg-gray-100 text-sm dark:bg-gray-900 dark:hover:bg-gray-700"
                  >
                    <div className="flex items-center gap-2">
                      <FileText className="w-4 h-4 text-gray-400 shrink-0" />
                      <span className="truncate">{chunk.file_path}</span>
                    </div>
                    <div className="text-xs text-gray-500 mt-1">
                      Score: {chunk.score.toFixed(2)}
                    </div>
                  </button>
                ))}
              </div>
            )}
          </div>
        )}

        {!isUser && (
          <div className="mt-2 flex gap-2">
            <button
              onClick={handleCopy}
              className="text-xs text-gray-500 hover:text-gray-700 flex items-center gap-1 dark:hover:text-gray-300"
            >
              {copied ? <Check className="w-3 h-3" /> : <Copy className="w-3 h-3" />}
              {copied ? '已复制' : '复制'}
            </button>
          </div>
        )}

        <div className={cn('text-xs mt-1', isUser ? 'text-blue-200' : 'text-gray-400')}>
          {new Date(message.timestamp).toLocaleTimeString('zh-CN')}
        </div>
      </div>
    </div>
  )
}
```

- [ ] **Step 3: 创建 src/components/MessageList.tsx**

```typescript
import { useEffect, useRef } from 'react'
import type { Message, ChunkReference } from '@/types'
import { MessageItem } from './MessageItem'
import { ScrollArea } from './ui/scroll-area'

interface MessageListProps {
  messages: Message[]
  isLoading: boolean
  onChunkClick?: (chunk: ChunkReference) => void
}

export const MessageList = ({ messages, isLoading, onChunkClick }: MessageListProps) => {
  const bottomRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [messages, isLoading])

  return (
    <ScrollArea className="flex-1 px-2 md:px-4">
      <div className="py-4 space-y-2 max-w-4xl mx-auto">
        {messages.length === 0 ? (
          <div className="text-center text-gray-500 dark:text-gray-400 py-12">
            <p className="text-lg">开始对话</p>
            <p className="text-sm mt-2">输入你的问题，AI 将基于知识库回答</p>
          </div>
        ) : (
          messages.map((message) => (
            <MessageItem
              key={message.id}
              message={message}
              onChunkClick={onChunkClick}
            />
          ))
        )}

        {isLoading && (
          <div className="flex gap-3 p-4">
            <div className="bg-gray-100 dark:bg-gray-800 rounded-lg px-4 py-3">
              <div className="flex gap-1">
                <span className="w-2 h-2 bg-gray-400 rounded-full animate-bounce" />
                <span className="w-2 h-2 bg-gray-400 rounded-full animate-bounce" style={{ animationDelay: '0.1s' }} />
                <span className="w-2 h-2 bg-gray-400 rounded-full animate-bounce" style={{ animationDelay: '0.2s' }} />
              </div>
            </div>
          </div>
        )}

        <div ref={bottomRef} />
      </div>
    </ScrollArea>
  )
}
```

- [ ] **Step 4: 创建 src/components/InputBox.tsx**

```typescript
import { useState, KeyboardEvent } from 'react'
import { Button } from './ui/button'
import { Send, Loader2 } from 'lucide-react'

interface InputBoxProps {
  onSend: (message: string) => void
  isLoading: boolean
  placeholder?: string
}

export const InputBox = ({ onSend, isLoading, placeholder = '输入你的问题... (Enter 发送, Shift+Enter 换行)' }: InputBoxProps) => {
  const [input, setInput] = useState('')

  const handleSend = () => {
    if (input.trim() && !isLoading) {
      onSend(input.trim())
      setInput('')
    }
  }

  const handleKeyDown = (e: KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault()
      handleSend()
    }
  }

  return (
    <div className="border-t border-gray-200 bg-white p-4 dark:border-gray-700 dark:bg-gray-800">
      <div className="flex gap-2 max-w-4xl mx-auto">
        <textarea
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder={placeholder}
          disabled={isLoading}
          className="flex-1 min-h-[44px] max-h-32 px-3 py-2 rounded-md border border-gray-300 bg-white text-sm resize-none focus:outline-none focus:ring-2 focus:ring-blue-500 dark:border-gray-600 dark:bg-gray-900"
          rows={1}
        />
        <Button
          onClick={handleSend}
          disabled={!input.trim() || isLoading}
          size="icon"
        >
          {isLoading ? (
            <Loader2 className="w-4 h-4 animate-spin" />
          ) : (
            <Send className="w-4 h-4" />
          )}
        </Button>
      </div>
    </div>
  )
}
```

- [ ] **Step 5: 创建 src/components/Chat.tsx**

```typescript
import { useCallback } from 'react'
import { useChat } from '@/hooks/useChat'
import { useRAG } from '@/hooks/useRAG'
import { MessageList } from './MessageList'
import { InputBox } from './InputBox'
import type { QueryOptions, ChunkReference } from '@/types'

interface ChatProps {
  options: QueryOptions
  buildQuery?: (content: string) => string  // Task 11 注入上下文拼接
  onChunkClick?: (chunk: ChunkReference) => void
  onAssistantMessage?: (content: string) => void  // Task 11 记录上下文
}

export const Chat = ({ options, buildQuery, onChunkClick, onAssistantMessage }: ChatProps) => {
  const { messages, isLoading, error, addMessage, setIsLoading, setError } = useChat()
  const { query } = useRAG()

  const handleSend = useCallback(async (content: string) => {
    addMessage({ role: 'user', content })

    setIsLoading(true)
    setError(null)

    try {
      const queryText = buildQuery ? buildQuery(content) : content
      const response = await query(queryText, options)

      addMessage({
        role: 'assistant',
        content: response.answer,
        chunks: response.chunks
      })
      onAssistantMessage?.(response.answer)
    } catch (err) {
      setError(err instanceof Error ? err.message : '查询失败')
    } finally {
      setIsLoading(false)
    }
  }, [addMessage, query, options, buildQuery, onAssistantMessage, setIsLoading, setError])

  return (
    <div className="flex flex-col h-full">
      {error && (
        <div className="bg-red-100 text-red-700 px-4 py-2 text-sm dark:bg-red-900/20 dark:text-red-400">
          {error}
        </div>
      )}

      <MessageList
        messages={messages}
        isLoading={isLoading}
        onChunkClick={onChunkClick}
      />

      <InputBox onSend={handleSend} isLoading={isLoading} />
    </div>
  )
}
```

- [ ] **Step 6: 类型检查 + Commit**

```bash
cd /d/code/book/web
npx tsc --noEmit
```

预期：无错误。

```bash
cd /d/code/book
git add web/src/hooks/useChat.ts web/src/components/
git commit -m "feat(web): 聊天组件与历史持久化"
```

---

## Task 9: 文档预览组件

**Files:**
- Create: `D:\code\book\web\src\components\DocumentPreview.tsx`

**Interfaces:**
- Consumes: `DocumentContent`, `Dialog`/`ScrollArea`/`Button` UI 组件, ReactMarkdown
- Produces: `DocumentPreview` 组件 props: `{ doc: DocumentContent, onClose: () => void }`

- [ ] **Step 1: 创建 src/components/DocumentPreview.tsx**

```typescript
import { Dialog, DialogContent, DialogHeader, DialogTitle } from './ui/dialog'
import { ScrollArea } from './ui/scroll-area'
import { Button } from './ui/button'
import ReactMarkdown from 'react-markdown'
import { Prism as SyntaxHighlighter } from 'react-syntax-highlighter'
import { vscDarkPlus } from 'react-syntax-highlighter/dist/esm/styles/prism'
import { useState } from 'react'
import type { DocumentContent } from '@/types'

interface DocumentPreviewProps {
  doc: DocumentContent
  onClose: () => void
}

export const DocumentPreview = ({ doc, onClose }: DocumentPreviewProps) => {
  const [copied, setCopied] = useState(false)

  const handleCopy = async () => {
    await navigator.clipboard.writeText(doc.file_path)
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }

  return (
    <Dialog open={true} onOpenChange={(open) => { if (!open) onClose() }}>
      <DialogContent className="max-w-4xl w-[95vw] max-h-[85vh] flex flex-col">
        <DialogHeader className="shrink-0">
          <DialogTitle className="truncate pr-8">{doc.title || doc.file_path}</DialogTitle>
          <div className="text-xs text-gray-500 truncate">{doc.file_path}</div>
          <div className="flex gap-2 mt-2">
            <Button variant="outline" size="sm" onClick={handleCopy}>
              {copied ? '已复制' : '复制路径'}
            </Button>
          </div>
        </DialogHeader>

        <ScrollArea className="flex-1 mt-2 border-t border-gray-200 pt-4 dark:border-gray-700">
          <div className="prose prose-sm dark:prose-invert max-w-none">
            <ReactMarkdown
              components={{
                code({ className, children, ...props }) {
                  const match = /language-(\w+)/.exec(className || '')
                  return match ? (
                    <SyntaxHighlighter
                      style={vscDarkPlus}
                      language={match[1]}
                      PreTag="div"
                    >
                      {String(children).replace(/\n$/, '')}
                    </SyntaxHighlighter>
                  ) : (
                    <code className={className} {...props}>
                      {children}
                    </code>
                  )
                }
              }}
            >
              {doc.content}
            </ReactMarkdown>
          </div>
        </ScrollArea>
      </DialogContent>
    </Dialog>
  )
}
```

- [ ] **Step 2: 类型检查 + Commit**

```bash
cd /d/code/book/web
npx tsc --noEmit
```

```bash
cd /d/code/book
git add web/src/components/DocumentPreview.tsx
git commit -m "feat(web): 文档预览模态组件"
```

---

## Task 10: 设置面板

**Files:**
- Create: `D:\code\book\web\src\components\SettingsPanel.tsx`

**Interfaces:**
- Consumes: `Settings` 类型, `Card`/`Slider`/`Switch`/`Button` 组件
- Produces: `SettingsPanel` 组件 props: `{ settings: Settings, onChange: (s: Settings) => void, onClose: () => void }`；`DEFAULT_SETTINGS` 常量

- [ ] **Step 1: 创建 src/components/SettingsPanel.tsx**

```typescript
import { useState, useEffect } from 'react'
import { Card, CardContent, CardHeader, CardTitle } from './ui/card'
import { Slider } from './ui/slider'
import { Switch } from './ui/switch'
import { Button } from './ui/button'
import { X, RotateCcw } from 'lucide-react'
import type { Settings } from '@/types'

export const DEFAULT_SETTINGS: Settings = {
  topK: 5,
  minScore: 0.1,
  temperature: 0.7,
  maxTokens: 1024,
  useRerank: true,
  maxTurns: 5
}

interface SettingsPanelProps {
  settings: Settings
  onChange: (settings: Settings) => void
  onClose: () => void
}

export const SettingsPanel = ({ settings, onChange, onClose }: SettingsPanelProps) => {
  const [localSettings, setLocalSettings] = useState<Settings>(settings)

  useEffect(() => {
    setLocalSettings(settings)
  }, [settings])

  const handleSave = () => {
    onChange(localSettings)
    localStorage.setItem('rag-settings', JSON.stringify(localSettings))
    onClose()
  }

  const handleReset = () => {
    setLocalSettings(DEFAULT_SETTINGS)
  }

  return (
    <div className="absolute right-0 top-0 h-full w-full sm:w-80 bg-white border-l border-gray-200 shadow-lg z-10 dark:bg-gray-800 dark:border-gray-700">
      <div className="flex items-center justify-between p-4 border-b border-gray-200 dark:border-gray-700">
        <h2 className="text-lg font-semibold">设置</h2>
        <button
          onClick={onClose}
          className="text-gray-500 hover:text-gray-700 dark:hover:text-gray-300"
        >
          <X className="w-5 h-5" />
        </button>
      </div>

      <div className="p-4 space-y-4 overflow-y-auto h-[calc(100%-130px)]">
        <Card>
          <CardHeader className="pb-2">
            <CardTitle className="text-sm">检索配置</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div>
              <label className="text-sm text-gray-600 dark:text-gray-400">
                Top K: {localSettings.topK}
              </label>
              <Slider
                value={[localSettings.topK]}
                onValueChange={([v]) => setLocalSettings(s => ({ ...s, topK: v }))}
                min={1}
                max={50}
                step={1}
                className="mt-2"
              />
            </div>

            <div>
              <label className="text-sm text-gray-600 dark:text-gray-400">
                最小分数: {localSettings.minScore.toFixed(2)}
              </label>
              <Slider
                value={[Math.round(localSettings.minScore * 100)]}
                onValueChange={([v]) => setLocalSettings(s => ({ ...s, minScore: v / 100 }))}
                min={0}
                max={100}
                step={5}
                className="mt-2"
              />
            </div>

            <div className="flex items-center justify-between">
              <label className="text-sm text-gray-600 dark:text-gray-400">
                启用 Rerank
              </label>
              <Switch
                checked={localSettings.useRerank}
                onCheckedChange={(checked) => setLocalSettings(s => ({ ...s, useRerank: checked }))}
              />
            </div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="pb-2">
            <CardTitle className="text-sm">生成配置</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div>
              <label className="text-sm text-gray-600 dark:text-gray-400">
                温度: {localSettings.temperature.toFixed(2)}
              </label>
              <Slider
                value={[Math.round(localSettings.temperature * 100)]}
                onValueChange={([v]) => setLocalSettings(s => ({ ...s, temperature: v / 100 }))}
                min={0}
                max={200}
                step={10}
                className="mt-2"
              />
            </div>

            <div>
              <label className="text-sm text-gray-600 dark:text-gray-400">
                最大 Token: {localSettings.maxTokens}
              </label>
              <Slider
                value={[localSettings.maxTokens]}
                onValueChange={([v]) => setLocalSettings(s => ({ ...s, maxTokens: v }))}
                min={128}
                max={4096}
                step={128}
                className="mt-2"
              />
            </div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="pb-2">
            <CardTitle className="text-sm">对话配置</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div>
              <label className="text-sm text-gray-600 dark:text-gray-400">
                上下文轮数: {localSettings.maxTurns}
              </label>
              <Slider
                value={[localSettings.maxTurns]}
                onValueChange={([v]) => setLocalSettings(s => ({ ...s, maxTurns: v }))}
                min={0}
                max={20}
                step={1}
                className="mt-2"
              />
              <p className="text-xs text-gray-400 mt-1">0 = 不携带历史对话</p>
            </div>
          </CardContent>
        </Card>
      </div>

      <div className="absolute bottom-0 left-0 right-0 p-4 border-t border-gray-200 bg-white flex gap-2 dark:border-gray-700 dark:bg-gray-800">
        <Button variant="outline" onClick={handleReset} className="flex-1">
          <RotateCcw className="w-4 h-4 mr-1" />
          重置
        </Button>
        <Button onClick={handleSave} className="flex-1">
          保存
        </Button>
      </div>
    </div>
  )
}
```

- [ ] **Step 2: 类型检查 + Commit**

```bash
cd /d/code/book/web
npx tsc --noEmit
```

```bash
cd /d/code/book
git add web/src/components/SettingsPanel.tsx
git commit -m "feat(web): 设置面板（检索/生成/对话配置）"
```

---

## Task 11: 多轮对话上下文

**Files:**
- Create: `D:\code\book\web\src\hooks\useContext.ts`

**Interfaces:**
- Consumes: Chat 组件的 `buildQuery` / `onAssistantMessage` props（Task 8 已定义）
- Produces: `useContext(maxTurns) → { buildQuery, recordTurn, clearContext }`；`buildQuery(content)` 返回拼接历史后的查询字符串

- [ ] **Step 1: 创建 src/hooks/useContext.ts**

```typescript
import { useRef, useCallback } from 'react'

interface Turn {
  user: string
  assistant: string
}

// 多轮对话上下文管理（设计文档 3.2.4：前端拼接 prompt 方案）
export const useContext = (maxTurns: number) => {
  const historyRef = useRef<Turn[]>([])

  // 记录一轮完整对话（在收到 AI 回复后调用）
  const recordTurn = useCallback((user: string, assistant: string) => {
    historyRef.current.push({ user, assistant })
    if (historyRef.current.length > maxTurns) {
      historyRef.current = historyRef.current.slice(-maxTurns)
    }
  }, [maxTurns])

  // 构建带上下文的查询
  const buildQuery = useCallback((currentQuery: string): string => {
    const history = historyRef.current.slice(-maxTurns)
    if (maxTurns <= 0 || history.length === 0) {
      return currentQuery
    }

    const context = history
      .map(t => `用户: ${t.user}\n助手: ${t.assistant}`)
      .join('\n')

    return `以下是对话历史:\n${context}\n\n用户当前问题: ${currentQuery}\n\n请基于对话历史和当前问题提供回答。`
  }, [maxTurns])

  const clearContext = useCallback(() => {
    historyRef.current = []
  }, [])

  return { buildQuery, recordTurn, clearContext }
}
```

- [ ] **Step 2: 类型检查 + Commit**

```bash
cd /d/code/book/web
npx tsc --noEmit
```

```bash
cd /d/code/book
git add web/src/hooks/useContext.ts
git commit -m "feat(web): 多轮对话上下文 hook（prompt 拼接方案）"
```

---

## Task 12: App 集成（主题/新对话/预览接线）

**Files:**
- Create: `D:\code\book\web\src\components\ThemeToggle.tsx`
- Modify: `D:\code\book\web\src\App.tsx`（替换 Task 4 的占位版本）

**Interfaces:**
- Consumes: `Chat`, `SettingsPanel` + `DEFAULT_SETTINGS`, `ThemeToggle`, `useContext`, `useDocument`, `DocumentPreview`, `Settings`, `QueryOptions`, `ChunkReference`
- Produces: 完整应用；`App` 无 props

- [ ] **Step 1: 创建 src/components/ThemeToggle.tsx**

```typescript
import { useState, useEffect } from 'react'
import { Moon, Sun } from 'lucide-react'
import { Button } from './ui/button'

export const ThemeToggle = () => {
  const [theme, setTheme] = useState<'light' | 'dark'>(() => {
    const saved = localStorage.getItem('theme')
    if (saved === 'light' || saved === 'dark') return saved
    return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'
  })

  useEffect(() => {
    document.documentElement.classList.toggle('dark', theme === 'dark')
    localStorage.setItem('theme', theme)
  }, [theme])

  const toggleTheme = () => {
    setTheme(t => t === 'light' ? 'dark' : 'light')
  }

  return (
    <Button variant="ghost" size="icon" onClick={toggleTheme} title="切换主题">
      {theme === 'light' ? <Moon className="w-5 h-5" /> : <Sun className="w-5 h-5" />}
    </Button>
  )
}
```

- [ ] **Step 2: 创建完整 src/App.tsx（替换占位版本）**

```typescript
import { useState, useCallback, useRef } from 'react'
import { QueryClient, QueryClientProvider } from '@tanstack/react-query'
import { Chat } from './components/Chat'
import { SettingsPanel, DEFAULT_SETTINGS } from './components/SettingsPanel'
import { ThemeToggle } from './components/ThemeToggle'
import { DocumentPreview } from './components/DocumentPreview'
import { Button } from './components/ui/button'
import { useContext } from './hooks/useContext'
import { useDocument } from './hooks/useDocument'
import { Settings, MessageSquare, RotateCcw, Loader2 } from 'lucide-react'
import type { Settings as SettingsType, QueryOptions, ChunkReference } from './types'

const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      retry: 1,
      refetchOnWindowFocus: false,
    },
  },
})

function App() {
  const [settings, setSettings] = useState<SettingsType>(() => {
    try {
      const saved = localStorage.getItem('rag-settings')
      return saved ? { ...DEFAULT_SETTINGS, ...JSON.parse(saved) } : DEFAULT_SETTINGS
    } catch {
      return DEFAULT_SETTINGS
    }
  })
  const [showSettings, setShowSettings] = useState(false)
  const [chatKey, setChatKey] = useState(0)

  const { buildQuery, recordTurn, clearContext } = useContext(settings.maxTurns)
  const { previewDocument, isLoading: isDocLoading, loadDocument, closePreview } = useDocument()

  // 暂存当前用户问题，收到回复后与答案一起记入上下文
  const pendingQuestionRef = useRef<string>('')

  const queryOptions: QueryOptions = {
    topK: settings.topK,
    temperature: settings.temperature,
    maxTokens: settings.maxTokens,
    useRerank: settings.useRerank
  }

  const handleBuildQuery = useCallback((content: string): string => {
    pendingQuestionRef.current = content
    return buildQuery(content)
  }, [buildQuery])

  const handleAssistantMessage = useCallback((answer: string) => {
    if (pendingQuestionRef.current) {
      recordTurn(pendingQuestionRef.current, answer)
      pendingQuestionRef.current = ''
    }
  }, [recordTurn])

  const handleChunkClick = useCallback((chunk: ChunkReference) => {
    // 优先用 document_id，退化为 file_path（后端两种都支持）
    loadDocument(chunk.document_id || chunk.file_path)
  }, [loadDocument])

  const handleNewChat = () => {
    clearContext()
    localStorage.removeItem('rag-chat-history')
    setChatKey(k => k + 1)  // 重新挂载 Chat，清空消息
  }

  return (
    <QueryClientProvider client={queryClient}>
      <div className="h-full flex flex-col bg-gray-50 dark:bg-gray-900">
        <header className="flex items-center justify-between px-4 py-3 bg-white border-b border-gray-200 dark:bg-gray-800 dark:border-gray-700">
          <div className="flex items-center gap-2">
            <MessageSquare className="w-6 h-6 text-blue-600" />
            <h1 className="text-lg md:text-xl font-bold">D-code-book RAG</h1>
          </div>

          <div className="flex items-center gap-1 md:gap-2">
            <Button variant="ghost" size="icon" onClick={handleNewChat} title="新对话">
              <RotateCcw className="w-5 h-5" />
            </Button>
            <ThemeToggle />
            <Button
              variant="ghost"
              size="icon"
              onClick={() => setShowSettings(!showSettings)}
              title="设置"
            >
              <Settings className="w-5 h-5" />
            </Button>
          </div>
        </header>

        <main className="flex-1 relative overflow-hidden">
          <Chat
            key={chatKey}
            options={queryOptions}
            buildQuery={handleBuildQuery}
            onAssistantMessage={handleAssistantMessage}
            onChunkClick={handleChunkClick}
          />

          {isDocLoading && (
            <div className="absolute inset-0 flex items-center justify-center bg-black/20 z-20">
              <Loader2 className="w-8 h-8 animate-spin text-white" />
            </div>
          )}

          {showSettings && (
            <SettingsPanel
              settings={settings}
              onChange={setSettings}
              onClose={() => setShowSettings(false)}
            />
          )}
        </main>

        <footer className="px-4 py-2 text-center text-xs text-gray-500 border-t border-gray-200 dark:text-gray-400 dark:border-gray-700">
          D-code-book RAG
        </footer>

        {previewDocument && (
          <DocumentPreview
            doc={previewDocument}
            onClose={closePreview}
          />
        )}
      </div>
    </QueryClientProvider>
  )
}

export default App
```

- [ ] **Step 3: 类型检查 + 构建**

```bash
cd /d/code/book/web
npx tsc --noEmit
npm run build
```

预期：tsc 无错误；Vite 构建成功，输出 `dist/index.html` 与 `dist/assets/*`。

- [ ] **Step 4: Commit**

```bash
cd /d/code/book
git add web/src/App.tsx web/src/components/ThemeToggle.tsx
git commit -m "feat(web): App 集成（主题切换/新对话/文档预览接线/多轮上下文）"
```

---

## Task 13: 端到端验证与 README

**Files:**
- Create: `D:\code\book\web\README.md`

**Interfaces:**
- Consumes: 全部前后端模块
- Produces: 验证记录与使用文档

- [ ] **Step 1: 后端最终语法验证**

```bash
cd /d/code/book/engineering/rag
g++ -fsyntax-only -std=c++17 -I include src/rag/server/server.cpp
```

预期：无错误输出。

- [ ] **Step 2: 前端生产构建**

```bash
cd /d/code/book/web
npm run build
ls dist/
```

预期：`dist/` 包含 `index.html` 和 `assets/`。

- [ ] **Step 3: 手动端到端验证（需要后端服务器二进制运行时）**

启动方式（任选其一）：
- 开发模式：终端 1 启动 C++ 服务器（端口 8080），终端 2 `cd web && npm run dev`，访问 http://localhost:5173
- 生产模式：`npm run build` 后，C++ 服务器配置 `ServerConfig.static_dir = "web/dist"`（或绝对路径），访问 http://localhost:8080

验收清单（对应设计文档第 11 节）：
- [ ] 提问并获得 RAG 答案
- [ ] 答案下方显示引用文档及 Score
- [ ] 点击引用卡片弹出完整文档预览
- [ ] 追问"它/那是什么"类问题时 AI 能结合上文（多轮上下文）
- [ ] 设置修改保存后刷新仍生效
- [ ] 深色模式切换正常
- [ ] 新对话按钮清空消息与上下文
- [ ] 手机宽度（<640px）下布局可用

注：本环境无法完整 CMake 构建后端（FetchContent 下载失败为已知问题），端到端验证需在能构建后端的环境执行。

- [ ] **Step 4: 创建 web/README.md**

```markdown
# RAG Web UI

D-code-book RAG 系统的 Web 交互界面。

## 开发

```bash
# 终端 1: 启动 C++ API 服务器（端口 8080）
# 终端 2:
npm install
npm run dev   # http://localhost:5173，/api 自动代理到 8080
```

## 构建与部署

```bash
npm run build   # 输出 dist/
```

将 `dist/` 路径配置为 C++ 服务器的 `ServerConfig.static_dir`，
即可由后端直接托管（http://localhost:8080）。

## 功能

- 对话界面（Markdown 渲染 + 代码高亮）
- 文档引用展示与完整文档预览
- 多轮对话上下文（可配置轮数）
- 参数配置（TopK / 温度 / Rerank 等，localStorage 持久化）
- 对话历史本地持久化
- 深色模式
- 响应式布局

## 技术栈

React 18 / TypeScript / Vite / Tailwind CSS / Radix UI / React Query

## API

前端调用后端 REST API：
- `POST /api/v1/query` — 提问（多轮上下文在前端拼接进 query）
- `GET /api/v1/documents/{id}/content` — 文档完整内容（预览）
- `GET /health` — 健康检查
```

- [ ] **Step 5: Commit**

```bash
cd /d/code/book
git add web/README.md
git commit -m "docs(web): Web UI 使用文档"
```

---

## 后续项（不在本计划范围）

1. **流式输出**：后端流式生成模块未实现（T22 已跳过）。实现后前端改为 SSE/分块读取，MessageItem 增量渲染。
2. **minScore / temperature / maxTokens 生效**：后端 `/api/v1/query` 当前只解析 `query` 和 `top_k`，其余设置项需后端支持后才能真正生效（当前仅 topK 生效，其他设置已持久化备用）。
3. **rag_server 可执行入口**：当前仓库没有调用 `create_server()` 的 main；需新增 server app 并配置 `static_dir`。

---

*计划版本：2.0.0（对照后端实际代码修正）*
*最后更新：2026-09-03*
