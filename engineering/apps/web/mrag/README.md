# Modular RAG Web UI

Modular RAG 框架的 Web 前端，基于 **Next.js 14** + **TypeScript** + **Tailwind CSS** + **shadcn/ui**。

## 功能特性

- 💬 **对话界面** - ChatGPT 风格的多对话管理，支持流式响应与检索上下文可视化
- 📚 **知识库管理** - 拖放上传、文档列表、分块状态跟踪
- ⚙️ **Pipeline 配置** - 9 种 RAG Pipeline 类型切换与参数配置
- 📊 **评估系统** - 测试集管理、评估运行、指标对比、优化建议
- 📈 **系统监控** - 实时查询、资源使用、延迟分布

## 技术栈

- **框架**: Next.js 14 (App Router) + React 18
- **语言**: TypeScript 5
- **样式**: Tailwind CSS + shadcn/ui
- **状态管理**: Zustand
- **数据请求**: TanStack Query (React Query)
- **图表**: ECharts (可选) / 自研组件
- **图标**: lucide-react
- **测试**: Vitest

## 目录结构

```
web/
├── src/
│   ├── app/                    # Next.js App Router
│   │   ├── layout.tsx          # 根布局
│   │   ├── page.tsx            # 主页面（视图切换）
│   │   └── globals.css         # 全局样式
│   ├── components/
│   │   ├── ui/                 # shadcn UI 原子组件
│   │   │   ├── button.tsx
│   │   │   ├── card.tsx
│   │   │   ├── input.tsx
│   │   │   ├── textarea.tsx
│   │   │   ├── badge.tsx
│   │   │   ├── progress.tsx
│   │   │   └── tabs.tsx
│   │   ├── chat/               # 对话相关组件
│   │   │   └── chat-interface.tsx
│   │   ├── knowledge/          # 知识库组件
│   │   │   └── knowledge-view.tsx
│   │   ├── pipeline/           # Pipeline 配置组件
│   │   │   └── pipeline-view.tsx
│   │   ├── evaluation/         # 评估组件
│   │   │   └── evaluation-view.tsx
│   │   ├── monitor/            # 监控组件
│   │   │   └── monitor-view.tsx
│   │   └── sidebar.tsx         # 左侧导航
│   ├── stores/                 # Zustand 状态管理
│   │   ├── chat-store.ts       # 对话状态
│   │   └── ui-store.ts         # UI 状态
│   └── lib/
│       ├── api.ts              # API 客户端
│       └── utils.ts            # 工具函数
├── package.json
├── tsconfig.json
├── tailwind.config.ts
├── next.config.js
└── README.md
```

## 快速开始

### 安装依赖

```bash
cd web
npm install
```

### 开发模式

```bash
npm run dev
```

默认运行在 `http://localhost:3000`

### 构建生产版本

```bash
npm run build
npm start
```

### 代码检查

```bash
npm run lint
npm run type-check
```

### 运行测试

```bash
npm run test
```

## 环境配置

创建 `.env.local`:

```bash
NEXT_PUBLIC_API_BASE_URL=http://localhost:8080
```

## 后端 API 接口

Web UI 通过 `src/lib/api.ts` 调用后端 REST API：

| 模块   | 接口                                   |
|---|---|
| 对话   | `POST /api/v1/chat/query`              |
| 知识库 | `POST /api/v1/documents`              |
| Pipeline | `GET /api/v1/pipelines`, `POST /api/v1/pipelines/{id}/config` |
| 评估   | `GET /api/v1/eval/datasets`, `POST /api/v1/eval/runs`, `GET /api/v1/eval/runs/{id}` |
| 监控   | `GET /api/v1/system/status`           |

## 视图说明

### 对话视图（chat）
- 多对话管理（创建/切换/删除）
- 流式回答（支持打字效果）
- 检索上下文可折叠查看
- 性能指标显示（检索/生成/总耗时）

### 知识库视图（knowledge）
- 拖放上传区
- 文档状态：`pending` / `processing` / `indexed` / `failed`
- 分块计数显示

### Pipeline 视图（pipeline）
- 9 种 Pipeline 可视化选择卡片
- 参数配置面板（Top K, Temperature, Max Tokens, RRF K）

### 评估视图（evaluation）
- 5 个 Tab：概览 / 测试集 / 运行历史 / 对比分析 / 优化建议
- 指标卡片（Precision, Recall, MRR, 幻觉率）
- 优化建议按严重度排序

### 监控视图（monitor）
- 系统状态卡（健康/QPS/延迟/错误率）
- 资源使用（CPU/内存/GPU/向量索引）
- 延迟分布直方图
- 最近查询列表

## 设计原则

1. **轻量级**: 仅依赖核心库，无冗余组件
2. **类型安全**: 全 TypeScript，接口契约清晰
3. **响应式**: 桌面优先，移动端可降级显示
4. **可扩展**: 新 Pipeline / 新指标易于添加
5. **可测试**: 组件可独立测试，stores 可单独验证

## License

MIT