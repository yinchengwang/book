"""
多模态 RAG 的 REST API 层

使用 FastAPI 提供 HTTP 接口，实现：
- 文档上传/查询/删除
- 多模态检索 + 生成
- 用户反馈
"""

import logging
from typing import Optional

try:
    from fastapi import FastAPI, HTTPException, UploadFile, File, Form, Header
    from pydantic import BaseModel
    FASTAPI_AVAILABLE = True
except ImportError:
    logger = logging.getLogger(__name__)
    logger.warning("fastapi 未安装，仅提供数据模型")
    FASTAPI_AVAILABLE = False

logger = logging.getLogger(__name__)


# ==================== 数据模型 ====================

if FASTAPI_AVAILABLE:

    class DocumentUploadResponse(BaseModel):
        """文档上传响应"""
        doc_id: int
        job_id: str
        title: str
        status: str
        created_at: str


    class DocumentDetail(BaseModel):
        """文档详情"""
        doc_id: int
        tenant_id: int
        title: str
        file_path: str
        version: int
        chunk_counts: dict
        metadata: dict
        created_at: str
        updated_at: str


    class IngestionStatus(BaseModel):
        """入库任务状态"""
        job_id: str
        status: str
        progress: int
        stages: list
        error: Optional[str] = None


    class QueryRequest(BaseModel):
        """检索请求"""
        tenant_id: int
        query: str
        modalities: list = ["text", "table", "image", "code"]
        top_k: int = 10
        session_id: Optional[str] = None


    class QueryResponse(BaseModel):
        """检索响应"""
        query: str
        answer: str
        sources: list
        token_count: int


    class FeedbackRequest(BaseModel):
        """用户反馈请求"""
        query_id: str
        tenant_id: int
        feedback_type: str   # thumbs_up / thumbs_down
        rating: Optional[int] = None  # 1-5
        comment: Optional[str] = None


    class FeedbackResponse(BaseModel):
        """用户反馈响应"""
        feedback_id: int
        message: str = "反馈已记录"

# ==================== FastAPI 应用 ====================

if FASTAPI_AVAILABLE:

    def create_app() -> FastAPI:
        """
        创建 FastAPI 应用

        Returns:
            FastAPI 实例
        """
        app = FastAPI(
            title="VDB Multi-modal RAG API",
            version="0.1.0",
            description="全模态 RAG API：表格/图像/代码/文本统一检索",
        )

        # 健康检查
        @app.get("/health")
        async def health_check():
            return {"status": "ok"}

        # POST /api/v1/documents — 文档上传
        @app.post("/api/v1/documents", response_model=DocumentUploadResponse)
        async def upload_document(
            file: UploadFile = File(...),
            tenant_id: int = Form(...),
            title: str = Form(None),
            metadata: str = Form("{}"),
            authorization: str = Header(None),
        ):
            """
            上传文档，自动触发多模态入库流水线
            """
            # TODO: 实现文档上传 + 入库任务入队
            return DocumentUploadResponse(
                doc_id=0,
                job_id="job_pending",
                title=title or file.filename,
                status="pending",
                created_at="",
            )

        # GET /api/v1/documents/ingest/status/{job_id} — 入库状态
        @app.get("/api/v1/documents/ingest/status/{job_id}", response_model=IngestionStatus)
        async def get_ingest_status(job_id: str):
            """查询入库任务进度"""
            # TODO: 实际查询 Redis 或数据库
            return IngestionStatus(
                job_id=job_id,
                status="processing",
                progress=50,
                stages=[],
            )

        # GET /api/v1/documents/{doc_id} — 文档详情
        @app.get("/api/v1/documents/{doc_id}", response_model=DocumentDetail)
        async def get_document(doc_id: int):
            """查询文档详情"""
            # TODO: 实际查询
            return DocumentDetail(
                doc_id=doc_id,
                tenant_id=1,
                title="Sample",
                file_path="",
                version=1,
                chunk_counts={"text": 0, "table": 0, "image": 0, "code": 0},
                metadata={},
                created_at="",
                updated_at="",
            )

        # DELETE /api/v1/documents/{doc_id} — 删除文档
        @app.delete("/api/v1/documents/{doc_id}")
        async def delete_document(doc_id: int):
            """删除文档及其所有 chunk"""
            # TODO: 调用 multimodal_delete_by_doc_id
            return {"code": 0, "message": "success", "data": {"deleted_chunks": 0}}

        # GET /api/v1/documents — 文档列表
        @app.get("/api/v1/documents")
        async def list_documents(
            tenant_id: int,
            page: int = 1,
            page_size: int = 20,
            keyword: str = None,
        ):
            """列出指定租户的所有文档"""
            # TODO: 实际查询
            return {
                "code": 0,
                "message": "success",
                "data": {
                    "items": [],
                    "pagination": {"page": page, "page_size": page_size, "total": 0, "total_pages": 0},
                },
            }

        # POST /api/v1/query — 多模态检索 + 生成
        @app.post("/api/v1/query", response_model=QueryResponse)
        async def query(request: QueryRequest):
            """
            多模态检索 + 生成

            流程:
            1. 路由（规则引擎 → LLM 意图分类）
            2. 指代消解
            3. 多路召回 + RRF 融合
            4. 上下文构建
            5. LLM 生成
            """
            # TODO: 实现完整流程
            return QueryResponse(
                query=request.query,
                answer="",
                sources=[],
                token_count=0,
            )

        # POST /api/v1/feedback — 用户反馈
        @app.post("/api/v1/feedback", response_model=FeedbackResponse)
        async def submit_feedback(request: FeedbackRequest):
            """记录用户反馈（点赞/点踩）"""
            # TODO: 记录到 user_feedback 表
            return FeedbackResponse(feedback_id=0, message="反馈已记录")

        return app


    app = create_app()
else:
    app = None