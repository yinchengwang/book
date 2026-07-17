from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from app.database import get_db
from app.models import User
from app.schemas.users import UserCreate, UserResponse

router = APIRouter(prefix="/api/v1/auth", tags=["认证"])


@router.post("/wechat", response_model=UserResponse)
def wechat_login(body: UserCreate, db: Session = Depends(get_db)):
    """微信小程序登录"""
    if not body.wechat_openid:
        raise HTTPException(status_code=400, detail="缺少 wechat_openid")

    user = db.query(User).filter(User.wechat_openid == body.wechat_openid).first()
    if not user:
        user = User(wechat_openid=body.wechat_openid)
        db.add(user)
        db.commit()
        db.refresh(user)

    return UserResponse.model_validate(user)


@router.post("/email", response_model=UserResponse)
def email_login(body: UserCreate, db: Session = Depends(get_db)):
    """Web 端邮箱注册/登录"""
    if not body.email:
        raise HTTPException(status_code=400, detail="缺少 email")

    user = db.query(User).filter(User.email == body.email).first()
    if not user:
        user = User(email=body.email)
        db.add(user)
        db.commit()
        db.refresh(user)

    return UserResponse.model_validate(user)
