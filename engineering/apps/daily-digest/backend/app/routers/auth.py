from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from typing import Optional
from sqlalchemy.orm import Session
from app.database import get_db
from app.models import User
from app.schemas.users import UserResponse


class WechatLoginRequest(BaseModel):
    code: str


class EmailLoginRequest(BaseModel):
    email: str


router = APIRouter(prefix="/api/v1/auth", tags=["认证"])


@router.post("/wechat", response_model=UserResponse)
def wechat_login(body: WechatLoginRequest, db: Session = Depends(get_db)):
    """微信小程序登录：用 code 换取 openid 后登录"""
    if not body.code:
        raise HTTPException(status_code=400, detail="缺少 code")

    # 简化实现：用 code 作为 openid 占位
    # 实际部署时应调用微信接口: https://api.weixin.qq.com/sns/jscode2session
    openid = body.code

    user = db.query(User).filter(User.wechat_openid == openid).first()
    if not user:
        user = User(wechat_openid=openid)
        db.add(user)
        db.commit()
        db.refresh(user)

    return UserResponse.model_validate(user)


@router.post("/email", response_model=UserResponse)
def email_login(body: EmailLoginRequest, db: Session = Depends(get_db)):
    """Web 端邮箱注册/登录"""
    if not body.email or "@" not in body.email:
        raise HTTPException(status_code=400, detail="无效的邮箱地址")

    user = db.query(User).filter(User.email == body.email).first()
    if not user:
        user = User(email=body.email)
        db.add(user)
        db.commit()
        db.refresh(user)

    return UserResponse.model_validate(user)
