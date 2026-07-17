from pydantic import BaseModel
from typing import Optional, List
from datetime import datetime


class CollectionCreate(BaseModel):
    user_id: int
    name: str
    description: Optional[str] = ""


class CollectionUpdate(BaseModel):
    name: Optional[str] = None
    description: Optional[str] = None


class CollectionResponse(BaseModel):
    id: int
    user_id: int
    name: str
    description: str
    item_count: Optional[int] = 0
    created_at: datetime

    model_config = {"from_attributes": True}


class CollectionItemCreate(BaseModel):
    item_id: int
    note: Optional[str] = ""


class CollectionItemResponse(BaseModel):
    id: int
    collection_id: int
    item_id: int
    note: str
    created_at: datetime

    model_config = {"from_attributes": True}