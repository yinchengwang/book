from pydantic import BaseModel
from typing import Optional, List


class PaginationParams(BaseModel):
    page: int = 1
    size: int = 20


class PaginationResponse(BaseModel):
    page: int
    size: int
    total: int


class MessageResponse(BaseModel):
    message: str
    detail: Optional[str] = None