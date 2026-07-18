from sqlalchemy import Column, Integer, String, Text, TIMESTAMP, ForeignKey, UniqueConstraint, JSON
from app.database import Base
from datetime import datetime


class Subscription(Base):
    """用户订阅方向表"""

    __tablename__ = "subscriptions"

    id = Column(Integer, primary_key=True, autoincrement=True)
    user_id = Column(Integer, ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    category = Column(String(32), nullable=False)
    keywords = Column(JSON, default=[])  # 标签数组
    weight = Column(Integer, default=0)
    created_at = Column(TIMESTAMP, default=datetime.utcnow)

    __table_args__ = (
        UniqueConstraint("user_id", "category", name="uq_user_category"),
    )