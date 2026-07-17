from pydantic import BaseModel
from typing import Optional
from datetime import datetime


class UserCreate(BaseModel):
    wechat_openid: Optional[str] = None
    email: Optional[str] = None


class UserPreferences(BaseModel):
    push_hour: Optional[int] = 8
    push_frequency: Optional[str] = "daily"  # daily / weekly / realtime
    categories: Optional[list[str]] = ["db", "ai"]


class UserResponse(BaseModel):
    id: int
    email: Optional[str] = None
    preferences: Optional[str] = "{}"
    created_at: datetime

    model_config = {"from_attributes": True}