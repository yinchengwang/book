#!/usr/bin/env python3
# =============================================================================
# fastapi 演示脚本
# 演示 FastAPI + 路由 + Pydantic
# =============================================================================

import sys
import os

# 检查依赖
try:
    from fastapi import FastAPI, HTTPException, Query, Path, Body
    from fastapi.responses import JSONResponse
    from pydantic import BaseModel, Field, validator
    from typing import Optional, List
    from datetime import datetime
except ImportError:
    print("错误：需要安装 fastapi 和 pydantic")
    print("运行: pip install fastapi uvicorn pydantic")
    sys.exit(1)

# =============================================================================
# 1. FastAPI 基础
# =============================================================================
def demo_basic_app():
    """演示 FastAPI 基础"""
    print("\n" + "=" * 60)
    print("1. FastAPI 基础")
    print("=" * 60)

    app = FastAPI(
        title="演示 API",
        description="FastAPI 基础功能演示",
        version="1.0.0"
    )

    @app.get("/")
    def root():
        """根路径"""
        return {"message": "Hello, FastAPI!", "version": "1.0.0"}

    @app.get("/health")
    def health():
        """健康检查"""
        return {"status": "healthy", "timestamp": datetime.now().isoformat()}

    print("✓ FastAPI 应用已创建")
    print("  • GET /           - 根路径")
    print("  • GET /health     - 健康检查")
    print("  • 运行命令: uvicorn main:app --reload")

# =============================================================================
# 2. Pydantic 模型
# =============================================================================
def demo_pydantic_models():
    """演示 Pydantic 数据模型"""
    print("\n" + "=" * 60)
    print("2. Pydantic 模型")
    print("=" * 60)

    # 定义用户模型
    class User(BaseModel):
        """用户模型"""
        id: int
        name: str = Field(..., min_length=1, max_length=100)
        email: str
        age: Optional[int] = None
        is_active: bool = True

        @validator('email')
        def validate_email(cls, v):
            if '@' not in v:
                raise ValueError('无效的邮箱格式')
            return v.lower()

    # 创建用户实例
    user = User(id=1, name="张三", email="Zhang@Example.com", age=25)
    print(f"✓ 用户模型示例：")
    print(f"  • ID: {user.id}")
    print(f"  • 姓名: {user.name}")
    print(f"  • 邮箱: {user.email}")  # 自动小写
    print(f"  • 年龄: {user.age}")
    print(f"  • 活跃: {user.is_active}")

    # JSON 序列化
    print(f"  • JSON: {user.json()}")

# =============================================================================
# 3. 路由和参数
# =============================================================================
def demo_routing():
    """演示路由和参数处理"""
    print("\n" + "=" * 60)
    print("3. 路由和参数")
    print("=" * 60)

    # 定义请求/响应模型
    class Item(BaseModel):
        """物品模型"""
        name: str
        description: Optional[str] = None
        price: float
        quantity: int = 0

    class ItemResponse(BaseModel):
        """物品响应模型"""
        id: int
        item: Item
        total: float

    print("✓ 路由示例：")
    print("")
    print("  # 路径参数")
    print("  @app.get('/items/{item_id}')")
    print("  def get_item(item_id: int = Path(..., ge=1)):")
    print("")
    print("  # 查询参数")
    print("  @app.get('/items')")
    print("  def list_items(skip: int = 0, limit: int = 10):")
    print("")
    print("  # 请求体")
    print("  @app.post('/items')")
    print("  def create_item(item: Item):")
    print("      return ItemResponse(id=1, item=item, total=item.price * item.quantity)")

# =============================================================================
# 4. 错误处理
# =============================================================================
def demo_error_handling():
    """演示错误处理"""
    print("\n" + "=" * 60)
    print("4. 错误处理")
    print("=" * 60)

    print("✓ 错误处理示例：")
    print("")
    print("  @app.get('/users/{user_id}')")
    print("  def get_user(user_id: int):")
    print("      if user_id not in db:")
    print("          raise HTTPException(status_code=404, detail='用户不存在')")
    print("      return db[user_id]")
    print("")
    print("  # 自定义异常处理器")
    print("  @app.exception_handler(ValueError)")
    print("  def value_error_handler(request, exc):")
    print("      return JSONResponse(status_code=400, content={'error': str(exc)})")

# =============================================================================
# 5. FastAPI vs Flask 对比
# =============================================================================
def demo_comparison():
    """FastAPI vs Flask 对比"""
    print("\n" + "=" * 60)
    print("5. FastAPI vs Flask")
    print("=" * 60)

    print("| 特性        | FastAPI              | Flask               |")
    print("|-------------|----------------------|---------------------|")
    print("| 类型安全    | 自动生成 OpenAPI      | 手动编写            |")
    print("| 数据验证    | Pydantic 内置        | WTForms 等          |")
    print("| 异步支持    | 原生 async/await     | 需要扩展            |")
    print("| 自动文档    | /docs, /redoc        | 需要 flask-restx    |")
    print("| 性能        | 接近 Node.js         | 较慢                |")
    print("| 学习曲线    | 中等                 | 较低                |")

# =============================================================================
# 主函数
# =============================================================================
def main():
    print("\n" + "╔" + "=" * 58 + "╗")
    print("║" + " " * 15 + "FastAPI Web 框架演示" + " " * 19 + "║")
    print("║" + " " * 12 + "路由、Pydantic、类型安全" + " " * 10 + "║")
    print("╚" + "=" * 58 + "╝")

    demo_basic_app()
    demo_pydantic_models()
    demo_routing()
    demo_error_handling()
    demo_comparison()

    print("\n" + "=" * 60)
    print("演示完成！关键知识点：")
    print("=" * 60)
    print("• from fastapi import FastAPI       # 创建应用")
    print("• @app.get/post/put/delete         # 定义路由")
    print("• from pydantic import BaseModel   # 数据验证")
    print("• uvicorn main:app --reload        # 启动服务")
    print("• /docs                            # 自动文档")
    print("=" * 60)
    print("\n运行完整 Web 服务：")
    print("  pip install fastapi uvicorn")
    print("  uvicorn main:app --reload")
    print("  访问 http://localhost:8000/docs")

if __name__ == '__main__':
    main()
