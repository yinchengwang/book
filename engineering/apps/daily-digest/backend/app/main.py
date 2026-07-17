from fastapi import FastAPI
from app.config import settings
import logging

logging.basicConfig(level=getattr(logging, settings.log_level.upper(), logging.INFO))
logger = logging.getLogger(__name__)

app = FastAPI(
    title="DailyDigest API",
    description="每日数据库 & AI 技术速览",
    version="0.1.0",
)


@app.get("/health")
def health():
    return {"status": "ok", "version": "0.1.0"}