from pydantic import BaseModel
from typing import Optional, List, Any
from datetime import datetime


class ItemBase(BaseModel):
    source: str
    source_id: str
    title: str
    url: str
    summary: Optional[str] = ""
    raw_content: Optional[str] = ""
    category: Optional[str] = "other"
    tags: Optional[List[str]] = []
    score: Optional[float] = 0.0
    published: Optional[datetime] = None


class ItemCreate(ItemBase):
    pass


class ItemResponse(ItemBase):
    id: int
    tags: Any  # JSON 字段可能返回为 list 或 str，兼容处理
    created_at: datetime

    model_config = {"from_attributes": True}


class ItemList(BaseModel):
    items: List[ItemResponse]
    total: int
    page: int
    size: int