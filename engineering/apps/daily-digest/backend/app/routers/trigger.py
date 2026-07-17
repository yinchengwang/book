from fastapi import APIRouter
import logging
from app.database import SessionLocal
from app.models import Item
from app.services.aggregator.orchestrator import AggregatorOrchestrator
from app.services.processor.classifier import Classifier
from app.services.processor.scorer import Scorer
from app.config import settings

logger = logging.getLogger("routers.trigger")
router = APIRouter(prefix="/api/v1/trigger", tags=["手动触发"])

_is_running = False


@router.post("/refresh")
async def trigger_refresh():
    """手动触发一次数据聚合"""
    global _is_running
    if _is_running:
        return {"message": "聚合任务已在运行中", "status": "running"}

    _is_running = True
    try:
        orchestrator = AggregatorOrchestrator(timeout=settings.fetch_timeout)
        items = await orchestrator.fetch_all()

        classifier = Classifier()
        scorer = Scorer()

        db = SessionLocal()
        try:
            new_count = 0
            for item in items:
                existing = db.query(Item).filter(
                    Item.source == item.source,
                    Item.source_id == item.source_id,
                ).first()
                if existing:
                    continue

                category, tags = classifier.process(item)
                score = scorer.score(item)

                db_item = Item(
                    source=item.source,
                    source_id=item.source_id,
                    title=item.title,
                    url=item.url,
                    summary=item.summary,
                    raw_content=item.raw_content,
                    category=category,
                    tags=str(tags),
                    score=score,
                    published=item.published,
                    source_weight=item.source_weight,
                )
                db.add(db_item)
                new_count += 1

            db.commit()
            logger.info(f"聚合完成: 新增 {new_count} 条，总计 {len(items)} 条原始数据")
            return {
                "message": "聚合完成",
                "status": "success",
                "total_raw": len(items),
                "new_items": new_count,
            }
        finally:
            db.close()
    except Exception as e:
        logger.error(f"聚合失败: {e}")
        return {"message": f"聚合失败: {str(e)}", "status": "error"}
    finally:
        _is_running = False
