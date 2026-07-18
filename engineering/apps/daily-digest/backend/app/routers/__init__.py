from app.routers.daily import router as daily_router
from app.routers.search import router as search_router
from app.routers.history import router as history_router
from app.routers.auth import router as auth_router
from app.routers.subscriptions import router as subscriptions_router
from app.routers.collections import router as collections_router
from app.routers.trigger import router as trigger_router
from app.routers.stats import router as stats_router
from app.routers.translate import router as translate_router

__all__ = [
    "daily_router",
    "search_router",
    "history_router",
    "auth_router",
    "subscriptions_router",
    "collections_router",
    "trigger_router",
    "stats_router",
    "translate_router",
]