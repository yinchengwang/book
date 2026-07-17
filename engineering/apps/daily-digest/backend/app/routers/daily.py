from fastapi import APIRouter, Depends, Query, HTTPException
from sqlalchemy.orm import Session
from typing import Optional
from app.database import get_db
from app.models import Item
from app.schemas.items import ItemResponse, ItemList

router = APIRouter(prefix="/api/v1/daily", tags=["每日速览"])


@router.get("", response_model=ItemList)
def get_daily(
    category: Optional[str] = Query(None, description="分类筛选"),
    page: int = Query(1, ge=1),
    size: int = Query(20, ge=1, le=100),
    db: Session = Depends(get_db),
):
    """获取当日速览内容"""
    query = db.query(Item).order_by(Item.score.desc(), Item.created_at.desc())

    if category:
        query = query.filter(Item.category == category)

    total = query.count()
    items = query.offset((page - 1) * size).limit(size).all()

    return ItemList(
        items=[ItemResponse.model_validate(i) for i in items],
        total=total,
        page=page,
        size=size,
    )


@router.get("/{item_id}", response_model=ItemResponse)
def get_item(item_id: int, db: Session = Depends(get_db)):
    """获取单条内容详情"""
    item = db.query(Item).filter(Item.id == item_id).first()
    if not item:
        raise HTTPException(status_code=404, detail="内容不存在")
    return ItemResponse.model_validate(item)
