from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session
from sqlalchemy import func
from datetime import datetime
from app.database import get_db
from app.models import Item, User, Subscription

router = APIRouter(prefix="/api/v1/stats", tags=["统计"])


@router.get("")
def get_stats(db: Session = Depends(get_db)):
    """获取系统统计信息"""
    total_items = db.query(Item).count()
    total_users = db.query(User).count()
    total_subscriptions = db.query(Subscription).count()

    today_start = datetime.utcnow().replace(hour=0, minute=0, second=0, microsecond=0)
    today_new = db.query(Item).filter(Item.created_at >= today_start).count()

    category_dist = (
        db.query(Item.category, func.count(Item.id))
        .group_by(Item.category)
        .all()
    )

    source_dist = (
        db.query(Item.source, func.count(Item.id))
        .group_by(Item.source)
        .all()
    )

    return {
        "total_items": total_items,
        "total_users": total_users,
        "total_subscriptions": total_subscriptions,
        "today_new": today_new,
        "category_distribution": {k: v for k, v in category_dist},
        "source_distribution": {k: v for k, v in source_dist},
    }
