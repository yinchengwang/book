from fastapi import APIRouter, Depends, Query
from sqlalchemy.orm import Session
from app.database import get_db
from app.models import Item, UserAction
from app.schemas.items import ItemResponse, ItemList

router = APIRouter(prefix="/api/v1/history", tags=["历史记录"])


@router.get("", response_model=ItemList)
def get_history(
    user_id: int = Query(..., description="用户 ID"),
    page: int = Query(1, ge=1),
    size: int = Query(20, ge=1, le=100),
    db: Session = Depends(get_db),
):
    """获取用户的阅读历史"""
    query = (
        db.query(Item)
        .join(UserAction, UserAction.item_id == Item.id)
        .filter(UserAction.user_id == user_id, UserAction.action == "view")
        .order_by(UserAction.created_at.desc())
    )

    total = query.count()
    items = query.offset((page - 1) * size).limit(size).all()

    return ItemList(
        items=[ItemResponse.model_validate(i) for i in items],
        total=total,
        page=page,
        size=size,
    )


@router.post("/record")
def record_action(
    user_id: int = Query(...),
    item_id: int = Query(...),
    action: str = Query("view", description="view / bookmark / click_through / share"),
    db: Session = Depends(get_db),
):
    """记录用户行为"""
    record = UserAction(user_id=user_id, item_id=item_id, action=action)
    db.add(record)
    db.commit()
    return {"message": "ok"}
