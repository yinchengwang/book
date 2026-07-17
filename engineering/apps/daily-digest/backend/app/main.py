from fastapi import FastAPI
from app.config import settings
from app.routers import (
    daily_router, search_router, history_router,
    auth_router, subscriptions_router, collections_router,
    trigger_router, stats_router,
)
from app.scheduler import start_scheduler, stop_scheduler
import logging

logging.basicConfig(level=getattr(logging, settings.log_level.upper(), logging.INFO))
logger = logging.getLogger(__name__)

app = FastAPI(
    title="DailyDigest API",
    description="每日数据库 & AI 技术速览",
    version="0.1.0",
)

# 注册路由
app.include_router(daily_router)
app.include_router(search_router)
app.include_router(history_router)
app.include_router(auth_router)
app.include_router(subscriptions_router)
app.include_router(collections_router)
app.include_router(trigger_router)
app.include_router(stats_router)


@app.get("/health")
def health():
    return {"status": "ok", "version": "0.1.0"}


@app.on_event("startup")
async def startup():
    logger.info("DailyDigest API 启动中...")
    start_scheduler()


@app.on_event("shutdown")
async def shutdown():
    logger.info("DailyDigest API 关闭中...")
    stop_scheduler()