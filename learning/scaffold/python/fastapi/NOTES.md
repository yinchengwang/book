# fastapi 学习笔记

## 概念地图

FastAPI 是现代 Python Web 框架，兼顾性能与开发体验：

- **FastAPI**：基于 Starlette，异步支持，原生类型安全
- **Pydantic**：数据验证和序列化，运行时类型检查
- **uvicorn**：ASGI 服务器，高性能异步运行

## 踩坑记录

1. **异步函数要用 async def**：同步函数用 def，异步函数用 async def
2. **Pydantic v2 语法变化**：BaseModel 配置方式有变化，注意版本
3. **文件上传**：需要使用 `UploadFile` 类型

## 工程对照（≥100 字硬约束）

### 1. Web API 设计模式

FastAPI 代表的现代 API 设计：

```python
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

app = FastAPI()

class Item(BaseModel):
    name: str
    price: float

@app.post("/items")
def create_item(item: Item):
    if item.price < 0:
        raise HTTPException(status_code=400, detail="价格不能为负")
    return {"id": 1, **item.dict()}
```

### 2. 与 Flask 的对比

| 场景 | FastAPI | Flask |
|------|---------|-------|
| 类型安全 | Pydantic 自动验证 | 手动验证 |
| 文档 | 自动生成 /docs | 需额外库 |
| 异步 | 原生支持 | 需扩展 |
| 性能 | 高 | 中 |

### 3. 生产部署

FastAPI + uvicorn 的生产配置：

```bash
# Gunicorn + uvicorn workers
gunicorn main:app -w 4 -k uvicorn.workers.UvicornWorker
```

学完本卡能动手的事：用 FastAPI 构建带类型验证的 REST API。
