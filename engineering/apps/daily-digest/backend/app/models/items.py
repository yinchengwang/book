from sqlalchemy import Column, Integer, String, Text, Float, Boolean, TIMESTAMP, UniqueConstraint, JSON
from app.database import Base
from datetime import datetime


class Item(Base):
    """聚合内容表"""

    __tablename__ = "items"

    id = Column(Integer, primary_key=True, autoincrement=True)
    source = Column(String(32), nullable=False, index=True)          # arxiv / blog / github / news / conference
    source_id = Column(String(128), nullable=False)                  # 原始 ID，用于去重
    title = Column(Text, nullable=False)
    url = Column(Text, nullable=False)
    summary = Column(Text, default="")                               # LLM 生成的摘要
    raw_content = Column(Text, default="")                           # 原文内容
    category = Column(String(32), default="other", index=True)       # db / ai / ml / infra / sys / other
    tags = Column(JSON, default=[])                                  # 标签数组
    score = Column(Float, default=0.0)                               # 重要度评分 [0, 1]
    published = Column(TIMESTAMP, nullable=True)                     # 原始发布时间
    source_weight = Column(Integer, default=0)                       # 数据源权重
    created_at = Column(TIMESTAMP, default=datetime.utcnow)
    is_push = Column(Boolean, default=False)

    __table_args__ = (
        UniqueConstraint("source", "source_id", name="uq_item_source_id"),
    )
