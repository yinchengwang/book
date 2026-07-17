from sqlalchemy import Column, Integer, String, TIMESTAMP, ForeignKey
from app.database import Base
from datetime import datetime


class UserAction(Base):
    __tablename__ = "user_actions"

    id = Column(Integer, primary_key=True, autoincrement=True)
    user_id = Column(Integer, ForeignKey("users.id"), nullable=False)
    item_id = Column(Integer, ForeignKey("items.id"), nullable=False)
    action = Column(String(16), nullable=False)  # view / bookmark / click_through / share
    created_at = Column(TIMESTAMP, default=datetime.utcnow)
