"""
Redis Stream 连接和任务队列模块

提供异步任务队列封装，基于 Redis Stream 实现：
- 任务入队 / 出队
- 进度查询
- 失败重试机制（最多 3 次）
"""

import json
import uuid
import time
import logging
from typing import Optional, Callable, Awaitable
from dataclasses import dataclass, field
from datetime import datetime

logger = logging.getLogger(__name__)


@dataclass
class Job:
    """入库任务"""
    job_id: str = ""
    doc_id: int = 0
    tenant_id: int = 0
    file_path: str = ""
    status: str = "pending"  # pending / processing / completed / failed
    progress: int = 0
    retry_count: int = 0
    max_retries: int = 3
    error: Optional[str] = None
    stages: list = field(default_factory=lambda: [
        {"name": "parsing", "status": "pending", "progress": 0},
        {"name": "chunking", "status": "pending", "progress": 0},
        {"name": "embedding", "status": "pending", "progress": 0},
        {"name": "indexing", "status": "pending", "progress": 0},
    ])
    created_at: str = ""
    updated_at: str = ""


class RedisStream:
    """Redis Stream 任务队列封装"""

    STREAM_KEY = "rag:ingestion"
    GROUP_NAME = "rag-workers"
    CONSUMER_NAME = "worker-1"

    def __init__(self, host: str = "localhost", port: int = 6379, db: int = 0):
        self.host = host
        self.port = port
        self.db = db
        self._redis = None

    async def connect(self):
        """连接 Redis"""
        try:
            import redis.asyncio as aioredis
            self._redis = await aioredis.from_url(
                f"redis://{self.host}:{self.port}/{self.db}",
                decode_responses=True
            )
            # 创建消费者组（如果不存在）
            try:
                await self._redis.xgroup_create(
                    self.STREAM_KEY, self.GROUP_NAME, id="0", mkstream=True
                )
            except Exception:
                # 组已存在，忽略
                pass
            logger.info(f"Redis 连接成功: {self.host}:{self.port}/{self.db}")
        except ImportError:
            logger.warning("redis.asyncio 未安装，使用模拟模式")
            self._redis = None
        except Exception as e:
            logger.error(f"Redis 连接失败: {e}")
            self._redis = None

    async def disconnect(self):
        """断开连接"""
        if self._redis:
            await self._redis.close()

    async def enqueue(self, job: Job) -> str:
        """
        将任务加入队列

        Args:
            job: 任务对象

        Returns:
            任务 ID
        """
        if not job.job_id:
            job.job_id = f"job_{uuid.uuid4().hex[:12]}"
        if not job.created_at:
            job.created_at = datetime.now().isoformat()
        job.updated_at = datetime.now().isoformat()

        data = {
            "job_id": job.job_id,
            "doc_id": str(job.doc_id),
            "tenant_id": str(job.tenant_id),
            "file_path": job.file_path,
            "status": job.status,
            "progress": str(job.progress),
            "retry_count": str(job.retry_count),
            "max_retries": str(job.max_retries),
            "stages": json.dumps(job.stages, ensure_ascii=False),
            "created_at": job.created_at,
            "updated_at": job.updated_at,
        }
        if job.error:
            data["error"] = job.error

        if self._redis:
            await self._redis.xadd(self.STREAM_KEY, data)
            # 同时存储任务详情用于进度查询
            await self._redis.set(
                f"rag:job:{job.job_id}",
                json.dumps(data, ensure_ascii=False),
                ex=86400,  # 24h 过期
            )
        else:
            # 模拟模式
            logger.info(f"[模拟] 任务入队: {job.job_id}")

        logger.info(f"任务入队成功: {job.job_id} (doc_id={job.doc_id})")
        return job.job_id

    async def dequeue(self, timeout_ms: int = 5000) -> Optional[Job]:
        """
        从队列中取出一个任务（阻塞读取）

        Args:
            timeout_ms: 超时时间（毫秒）

        Returns:
            Job 对象，超时返回 None
        """
        if not self._redis:
            logger.info("[模拟] 无任务可消费")
            return None

        try:
            results = await self._redis.xreadgroup(
                self.GROUP_NAME, self.CONSUMER_NAME,
                {self.STREAM_KEY: ">"},
                count=1,
                block=timeout_ms,
            )
        except Exception as e:
            logger.error(f"消费队列失败: {e}")
            return None

        if not results:
            return None

        stream_name, messages = results[0]
        if not messages:
            return None

        msg_id, data = messages[0]
        job = Job(
            job_id=data.get("job_id", ""),
            doc_id=int(data.get("doc_id", 0)),
            tenant_id=int(data.get("tenant_id", 0)),
            file_path=data.get("file_path", ""),
            status=data.get("status", "pending"),
            progress=int(data.get("progress", 0)),
            retry_count=int(data.get("retry_count", 0)),
            max_retries=int(data.get("max_retries", 3)),
            error=data.get("error"),
            stages=json.loads(data.get("stages", "[]")),
            created_at=data.get("created_at", ""),
            updated_at=data.get("updated_at", ""),
        )
        job._msg_id = msg_id  # type: ignore
        return job

    async def ack(self, job: Job):
        """确认任务完成"""
        if self._redis and hasattr(job, "_msg_id"):
            await self._redis.xack(self.STREAM_KEY, self.GROUP_NAME, job._msg_id)  # type: ignore
            await self._redis.xdel(self.STREAM_KEY, job._msg_id)  # type: ignore

    async def update_progress(self, job_id: str, progress: int, status: str = "processing"):
        """
        更新任务进度

        Args:
            job_id: 任务 ID
            progress: 进度百分比 (0-100)
            status: 状态
        """
        if self._redis:
            data = await self._redis.get(f"rag:job:{job_id}")
            if data:
                job_data = json.loads(data)
                job_data["progress"] = progress
                job_data["status"] = status
                job_data["updated_at"] = datetime.now().isoformat()
                await self._redis.set(
                    f"rag:job:{job_id}",
                    json.dumps(job_data, ensure_ascii=False),
                    ex=86400,
                )

    async def get_job_status(self, job_id: str) -> Optional[dict]:
        """
        查询任务状态

        Args:
            job_id: 任务 ID

        Returns:
            任务状态字典，不存在返回 None
        """
        if self._redis:
            data = await self._redis.get(f"rag:job:{job_id}")
            if data:
                return json.loads(data)
        return None


class IngestionWorker:
    """
    异步入库 Worker

    从 Redis Stream 消费任务，执行解析→切分→Embedding→索引
    """

    def __init__(self, redis_stream: RedisStream):
        self.redis = redis_stream
        self._running = False

    async def start(self):
        """启动 Worker"""
        self._running = True
        logger.info("入库 Worker 已启动")

        while self._running:
            try:
                job = await self.redis.dequeue(timeout_ms=3000)
                if job:
                    await self._process_job(job)
            except Exception as e:
                logger.error(f"Worker 处理异常: {e}")
                await asyncio.sleep(1)

    async def stop(self):
        """停止 Worker"""
        self._running = False
        logger.info("入库 Worker 已停止")

    async def _process_job(self, job: Job):
        """处理单个任务"""
        logger.info(f"开始处理任务: {job.job_id} (doc_id={job.doc_id})")

        try:
            # 阶段1: 解析
            await self.redis.update_progress(job.job_id, 10, "processing")
            # TODO: 调用解析器 parse_document(job.file_path)

            # 阶段2: 切分
            await self.redis.update_progress(job.job_id, 30, "processing")
            # TODO: 调用 chunker

            # 阶段3: Embedding
            await self.redis.update_progress(job.job_id, 60, "processing")
            # TODO: 调用 embedding 服务

            # 阶段4: 索引
            await self.redis.update_progress(job.job_id, 90, "processing")
            # TODO: 写入 VDB

            # 完成
            await self.redis.update_progress(job.job_id, 100, "completed")
            await self.redis.ack(job)
            logger.info(f"任务完成: {job.job_id}")

        except Exception as e:
            logger.error(f"任务失败: {job.job_id} - {e}")
            job.retry_count += 1
            if job.retry_count < job.max_retries:
                # 重试：重新入队
                job.error = str(e)
                job.status = "pending"
                await self.redis.enqueue(job)
                logger.info(f"任务重试 ({job.retry_count}/{job.max_retries}): {job.job_id}")
            else:
                # 超过最大重试次数
                job.error = str(e)
                await self.redis.update_progress(job.job_id, 0, "failed")
                logger.error(f"任务失败（超过最大重试次数）: {job.job_id}")


# 为了 async 支持
import asyncio