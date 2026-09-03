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