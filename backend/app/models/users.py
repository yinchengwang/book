from sqlalchemy import Column, Integer, String, Text, TIMESTAMP
from app.database import Base
from datetime import datetime


class User(Base):
    __tablename__ = "users"

    id = Column(Integer, primary_key=True, autoincrement=True)
    wechat_openid = Column(String(64), unique=True, nullable=True)
    email = Column(String(255), unique=True, nullable=True)
    preferences = Column(Text, default="{}")  # JSONB 降级为 Text JSON
    created_at = Column(TIMESTAMP, default=datetime.utcnow)
    last_active = Column(TIMESTAMP, default=datetime.utcnow)
