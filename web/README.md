# RAG Web UI — 界面实现

> RAG 系统的 React 前端，提供对话、文档管理和系统配置功能。

## 🏗️ 技术栈

| 类别 | 技术 |
|------|------|
| 框架 | React 18 + TypeScript |
| 构建 | Vite 5 |
| 样式 | Tailwind CSS 3.4 |
| 组件库 | Radix UI（无障碍） |
| 状态/数据 | React Query (@tanstack/react-query) |
| Markdown | react-markdown + react-syntax-highlighter |
| 图标 | Lucide React |

---

## 📁 组件结构

```
web/src/
├── App.tsx                    # 主应用（Tab 路由）
│
├── components/
│   ├── Chat.tsx              # 聊天组件（核心）
│   ├── MessageList.tsx       # 消息列表（Markdown 渲染）
│   ├── InputBox.tsx          # 输入框（支持上传）
│   │
│   ├── FileManager.tsx       # 文件管理器
│   │   ├── 文件列表 + 状态标签
│   │   ├── 面包屑导航
│   │   └── 工具栏（上传/重建索引）
│   │
│   ├── DocumentPreview.tsx   # 文档预览弹窗
│   ├── SettingsPanel.tsx     # 设置面板
│   │   ├── 检索配置（TopK、最小分数、Rerank）
│   │   ├── 生成配置（温度、最大Token）
│   │   └── 对话配置（上下文轮数）
│   │
│   ├── UploadDialog.tsx      # 上传对话框
│   ├── DirManager.tsx        # 目录管理
│   ├── RebuildDialog.tsx     # 重建索引对话框
│   ├── ThemeToggle.tsx       # 主题切换
│   │
│   └── ui/                   # 基础 UI 组件
│       ├── button.tsx
│       ├── card.tsx
│       ├── slider.tsx
│       ├── switch.tsx
│       └── dialog.tsx
│
├── hooks/
│   ├── useChat.ts            # 聊天状态管理
│   ├── useRAG.ts            # RAG API 调用
│   ├── useContext.ts        # 多轮对话上下文
│   └── useDocument.ts       # 文档加载
│
├── types/
│   └── index.ts             # TypeScript 类型定义
│
└── App.css                  # 全局样式
```

---

## 🔌 API 接口

前端通过 `/api` 代理调用后端（端口 8080）：

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/v1/query` | POST | 提问 |
| `/api/v1/documents` | GET | 文档列表 |
| `/api/v1/documents/{id}/content` | GET | 文档内容 |
| `/api/v1/documents` | DELETE | 删除文档 |
| `/api/v1/documents/dirs` | GET | 目录列表 |
| `/api/v1/documents/upload` | POST | 上传文档 |
| `/api/v1/index/rebuild` | POST | 重建索引 |
| `/health` | GET | 健康检查 |

---

## ⚙️ 设置项实现

```typescript
interface Settings {
  topK: number        // 1-50, 检索返回数
  minScore: number   // 0-1, 相关性阈值
  temperature: number // 0-2, LLM 随机性
  maxTokens: number   // 128-4096, 生成上限
  useRerank: boolean  // 是否启用重排序
  maxTurns: number    // 0-20, 上下文轮数
}
```

---

## 🎨 界面布局

```
┌──────────────────────────────────────────────────────────────┐
│  💬 D-code-book RAG   [对话] [文件管理]     🔄 🌙 ⚙️        │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│   ┌──────────────────────────────────────────────────────┐  │
│   │  用户: HNSW 索引如何构建？                            │  │
│   └──────────────────────────────────────────────────────┘  │
│                                                              │
│   ┌──────────────────────────────────────────────────────┐  │
│   │  AI: HNSW（分层可导航小世界图）是一种...             │  │
│   │                                                       │  │
│   │  📄 doc1.md (score: 0.95)  ← 点击预览                │  │
│   │  📄 doc2.md (score: 0.87)                            │  │
│   └──────────────────────────────────────────────────────┘  │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────┐   │
│  │ 输入问题...                                  [发送] 📎 │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

---

## 🔧 开发与构建

### 开发模式

```bash
# 终端 1: 启动后端（端口 8080）
./bin/rag_server

# 终端 2: 启动前端（端口 5173）
cd web
npm install
npm run dev
```

### 生产构建

```bash
npm run build   # 输出 dist/
```

将 `dist/` 配置为后端 `ServerConfig.static_dir`，即可由后端直接托管（`http://localhost:8080`）。

---

## 🌟 核心功能实现

### 1. 多轮对话上下文

```typescript
// useContext.ts — 上下文管理
const useContext = (maxTurns: number) => {
  // 历史消息：[{role: 'user'|'assistant', content: string}]
  // 发送时拼接历史："\n".join(history) + "\n" + currentQuery
};
```

### 2. Markdown 渲染

```typescript
// MessageList.tsx
<ReactMarkdown
  components={{
    code: SyntaxHighlighter,  // 代码高亮
  }}
>
  {message.content}
</ReactMarkdown>
```

### 3. 文档预览

```typescript
// useDocument.ts
const loadDocument = async (docId: string) => {
  const res = await fetch(`/api/v1/documents/${docId}/content`);
  const content = await res.text();
  // 渲染为 Markdown
};
```

### 4. 主题切换

```typescript
// ThemeToggle.tsx
const toggleTheme = () => {
  document.documentElement.classList.toggle('dark');
  localStorage.setItem('theme', isDark ? 'dark' : 'light');
};
```

---

## 📦 依赖清单

```json
{
  "dependencies": {
    "react": "^18.2.0",
    "@tanstack/react-query": "^5.0.0",
    "react-markdown": "^9.0.0",
    "react-syntax-highlighter": "^15.5.0",
    "@radix-ui/react-dialog": "^1.0.5",
    "@radix-ui/react-slider": "^1.1.2",
    "@radix-ui/react-switch": "^1.0.3",
    "lucide-react": "^0.300.0"
  }
}
```

---

## 🔗 相关链接

- [RAG 系统文档](../rag/README.md) — 后端架构与实现
- [RAG 设计文档](../rag/docs/) — 详细设计（10 篇）
- [根目录 README](../../README.md) — 项目整体介绍
