from sqlalchemy import Column, Integer, String, Text, TIMESTAMP
from app.database import Base
from datetime import datetime


class User(Base):
    """用户表"""

    __tablename__ = "users"

    id = Column(Integer, primary_key=True, autoincrement=True)
    wechat_openid = Column(String(64), unique=True, nullable=True)
    email = Column(String(255), unique=True, nullable=True)
    preferences = Column(Text, default="{}")  # JSON 字符串
    created_at = Column(TIMESTAMP, default=datetime.utcnow)
    last_active = Column(TIMESTAMP, default=datetime.utcnow)