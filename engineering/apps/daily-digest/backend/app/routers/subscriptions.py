from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from typing import List
from app.database import get_db
from app.models import Subscription
from app.schemas.subscriptions import SubscriptionCreate, SubscriptionUpdate, SubscriptionResponse

router = APIRouter(prefix="/api/v1/subscriptions", tags=["订阅管理"])


@router.get("", response_model=List[SubscriptionResponse])
def get_subscriptions(user_id: int, db: Session = Depends(get_db)):
    """获取用户订阅方向列表"""
    subs = db.query(Subscription).filter(Subscription.user_id == user_id).all()
    return [SubscriptionResponse.model_validate(s) for s in subs]


@router.post("", response_model=SubscriptionResponse)
def create_subscription(body: SubscriptionCreate, db: Session = Depends(get_db)):
    """新增订阅方向"""
    existing = db.query(Subscription).filter(
        Subscription.user_id == body.user_id,
        Subscription.category == body.category,
    ).first()
    if existing:
        raise HTTPException(status_code=409, detail="该订阅方向已存在")

    sub = Subscription(**body.model_dump())
    db.add(sub)
    db.commit()
    db.refresh(sub)
    return SubscriptionResponse.model_validate(sub)


@router.put("/{sub_id}", response_model=SubscriptionResponse)
def update_subscription(sub_id: int, body: SubscriptionUpdate, db: Session = Depends(get_db)):
    """更新订阅"""
    sub = db.query(Subscription).filter(Subscription.id == sub_id).first()
    if not sub:
        raise HTTPException(status_code=404, detail="订阅不存在")

    update_data = body.model_dump(exclude_unset=True)
    for key, value in update_data.items():
        setattr(sub, key, value)
    db.commit()
    db.refresh(sub)
    return SubscriptionResponse.model_validate(sub)


@router.delete("/{sub_id}")
def delete_subscription(sub_id: int, db: Session = Depends(get_db)):
    """取消订阅"""
    sub = db.query(Subscription).filter(Subscription.id == sub_id).first()
    if not sub:
        raise HTTPException(status_code=404, detail="订阅不存在")

    db.delete(sub)
    db.commit()
    return {"message": "订阅已取消"}
