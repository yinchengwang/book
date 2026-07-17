from pydantic import BaseModel
from typing import Optional, List
from datetime import datetime


class SubscriptionCreate(BaseModel):
    user_id: int
    category: str
    keywords: Optional[List[str]] = []
    weight: Optional[int] = 0


class SubscriptionUpdate(BaseModel):
    category: Optional[str] = None
    keywords: Optional[List[str]] = None
    weight: Optional[int] = None


class SubscriptionResponse(BaseModel):
    id: int
    user_id: int
    category: str
    keywords: str  # JSON 字符串
    weight: int
    created_at: datetime

    model_config = {"from_attributes": True}