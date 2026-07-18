from pydantic import BaseModel
from typing import Optional, List, Any
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
    keywords: Any  # JSON 字段，可能返回 list 或 str
    weight: int
    created_at: datetime

    model_config = {"from_attributes": True}