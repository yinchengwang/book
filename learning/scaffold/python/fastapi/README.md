# fastapi - Python Web 框架

## 概述

本卡演示 FastAPI Web 框架：路由、Pydantic 数据验证、自动文档。

## 核心概念

| 概念 | 说明 |
|------|------|
| FastAPI | 现代 Python Web 框架，自动 OpenAPI 文档 |
| Pydantic | 数据验证库，类型安全的模型定义 |
| uvicorn | ASGI 服务器，FastAPI 的运行环境 |

## 快速开始

```bash
# 运行演示
make run

# 启动 Web 服务
make serve
# 访问 http://localhost:8000/docs
```

## 依赖

- Python 3.7+
- fastapi
- uvicorn
- pydantic

## 验证

```bash
make test
```
