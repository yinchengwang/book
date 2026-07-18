from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from typing import List
from app.database import get_db
from app.models import Collection, CollectionItem
from app.schemas.collections import (
    CollectionCreate, CollectionUpdate, CollectionResponse,
    CollectionItemCreate, CollectionItemResponse,
)

router = APIRouter(prefix="/api/v1/collections", tags=["收藏夹"])


@router.get("", response_model=List[CollectionResponse])
def get_collections(user_id: int, db: Session = Depends(get_db)):
    """获取用户的收藏夹列表（含数量）"""
    collections = db.query(Collection).filter(Collection.user_id == user_id).all()
    result = []
    for col in collections:
        count = db.query(CollectionItem).filter(CollectionItem.collection_id == col.id).count()
        resp = CollectionResponse.model_validate(col)
        resp.item_count = count
        result.append(resp)
    return result


@router.post("", response_model=CollectionResponse)
def create_collection(body: CollectionCreate, db: Session = Depends(get_db)):
    """新建收藏夹"""
    existing = db.query(Collection).filter(
        Collection.user_id == body.user_id,
        Collection.name == body.name,
    ).first()
    if existing:
        raise HTTPException(status_code=409, detail="收藏夹名称已存在")

    col = Collection(**body.model_dump())
    db.add(col)
    db.commit()
    db.refresh(col)
    resp = CollectionResponse.model_validate(col)
    resp.item_count = 0
    return resp


@router.put("/{col_id}", response_model=CollectionResponse)
def update_collection(col_id: int, body: CollectionUpdate, db: Session = Depends(get_db)):
    """修改收藏夹"""
    col = db.query(Collection).filter(Collection.id == col_id).first()
    if not col:
        raise HTTPException(status_code=404, detail="收藏夹不存在")

    update_data = body.model_dump(exclude_unset=True)
    for key, value in update_data.items():
        setattr(col, key, value)
    db.commit()
    db.refresh(col)
    return CollectionResponse.model_validate(col)


@router.delete("/{col_id}")
def delete_collection(col_id: int, db: Session = Depends(get_db)):
    """删除收藏夹"""
    col = db.query(Collection).filter(Collection.id == col_id).first()
    if not col:
        raise HTTPException(status_code=404, detail="收藏夹不存在")

    db.delete(col)
    db.commit()
    return {"message": "收藏夹已删除"}


@router.get("/{col_id}/items", response_model=List[CollectionItemResponse])
def get_collection_items(col_id: int, db: Session = Depends(get_db)):
    """获取收藏夹中的内容列表"""
    col = db.query(Collection).filter(Collection.id == col_id).first()
    if not col:
        raise HTTPException(status_code=404, detail="收藏夹不存在")

    items = db.query(CollectionItem).filter(CollectionItem.collection_id == col_id).all()
    return [CollectionItemResponse.model_validate(i) for i in items]


@router.post("/{col_id}/items", response_model=CollectionItemResponse)
def add_collection_item(col_id: int, body: CollectionItemCreate, db: Session = Depends(get_db)):
    """将内容加入收藏夹"""
    existing = db.query(CollectionItem).filter(
        CollectionItem.collection_id == col_id,
        CollectionItem.item_id == body.item_id,
    ).first()
    if existing:
        raise HTTPException(status_code=409, detail="该内容已在收藏夹中")

    ci = CollectionItem(collection_id=col_id, **body.model_dump())
    db.add(ci)
    db.commit()
    db.refresh(ci)
    return CollectionItemResponse.model_validate(ci)


@router.delete("/{col_id}/items/{item_id}")
def remove_collection_item(col_id: int, item_id: int, db: Session = Depends(get_db)):
    """从收藏夹移除内容"""
    ci = db.query(CollectionItem).filter(
        CollectionItem.collection_id == col_id,
        CollectionItem.item_id == item_id,
    ).first()
    if not ci:
        raise HTTPException(status_code=404, detail="收藏项不存在")

    db.delete(ci)
    db.commit()
    return {"message": "已从收藏夹移除"}
