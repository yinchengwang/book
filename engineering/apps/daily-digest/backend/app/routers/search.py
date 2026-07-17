from fastapi import APIRouter, Depends, Query
from sqlalchemy import or_
from sqlalchemy.orm import Session
from typing import Optional
from app.database import get_db
from app.models import Item
from app.schemas.items import ItemResponse, ItemList

router = APIRouter(prefix="/api/v1/search", tags=["搜索"])


@router.get("", response_model=ItemList)
def search(
    q: str = Query("", description="搜索关键词"),
    category: Optional[str] = Query(None, description="分类筛选"),
    page: int = Query(1, ge=1),
    size: int = Query(20, ge=1, le=100),
    db: Session = Depends(get_db),
):
    """全文搜索历史内容"""
    query = db.query(Item)

    if q:
        keyword = f"%{q}%"
        query = query.filter(
            or_(
                Item.title.ilike(keyword),
                Item.summary.ilike(keyword),
                Item.raw_content.ilike(keyword),
                Item.tags.ilike(keyword),
            )
        )

    if category:
        query = query.filter(Item.category == category)

    query = query.order_by(Item.score.desc(), Item.created_at.desc())

    total = query.count()
    items = query.offset((page - 1) * size).limit(size).all()

    return ItemList(
        items=[ItemResponse.model_validate(i) for i in items],
        total=total,
        page=page,
        size=size,
    )
