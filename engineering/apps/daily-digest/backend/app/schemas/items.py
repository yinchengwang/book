from pydantic import BaseModel
from typing import Optional, List
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
    created_at: datetime

    model_config = {"from_attributes": True}


class ItemList(BaseModel):
    items: List[ItemResponse]
    total: int
    page: int
    size: int