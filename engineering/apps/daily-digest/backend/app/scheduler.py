import asyncio
import json
import logging
from datetime import datetime

from apscheduler.schedulers.asyncio import AsyncIOScheduler
from apscheduler.triggers.cron import CronTrigger

from app.config import settings
from app.services.aggregator.orchestrator import AggregatorOrchestrator
from app.services.processor.classifier import Classifier
from app.services.processor.scorer import Scorer
from app.database import SessionLocal
from app.models import Item

logger = logging.getLogger("scheduler")

# 全局调度器实例
scheduler = AsyncIOScheduler()


async def daily_aggregate_job():
    """每日聚合任务：抓取 -> 处理 -> 入库"""
    logger.info("=" * 60)
    logger.info("开始每日聚合任务...")
    start_time = datetime.utcnow()

    orchestrator = AggregatorOrchestrator(timeout=settings.fetch_timeout)
    classifier = Classifier()
    scorer = Scorer()

    try:
        # 1. 并行抓取所有数据源
        items = await orchestrator.fetch_all()

        if not items:
            logger.warning("所有数据源均无返回，跳过入库")
            return

        # 2. 处理并入库
        db = SessionLocal()
        try:
            new_count = 0
            skip_count = 0

            for item in items:
                # 去重检查
                existing = db.query(Item).filter(
                    Item.source == item.source,
                    Item.source_id == item.source_id,
                ).first()
                if existing:
                    skip_count += 1
                    continue

                # 分类 + 标签
                category, tags = classifier.process(item)

                # 评分
                score = scorer.score(item)

                # 创建数据库记录
                db_item = Item(
                    source=item.source,
                    source_id=item.source_id,
                    title=item.title,
                    url=item.url,
                    summary=item.summary,
                    raw_content=item.raw_content,
                    category=category,
                    tags=json.dumps(tags, ensure_ascii=False),
                    score=score,
                    published=item.published,
                    source_weight=item.source_weight,
                )
                db.add(db_item)
                new_count += 1

            db.commit()

            elapsed = (datetime.utcnow() - start_time).total_seconds()
            logger.info(f"每日聚合完成: 耗时 {elapsed:.1f}s")
            logger.info(f"  原始数据: {len(items)} 条")
            logger.info(f"  新增入库: {new_count} 条")
            logger.info(f"  跳过重复: {skip_count} 条")

            # 打印分类统计
            stats = orchestrator.get_stats(items)
            logger.info(f"  来源分布: {stats['by_source']}")
            logger.info(f"  分类分布: {stats['by_category']}")

        except Exception as e:
            db.rollback()
            logger.error(f"入库失败: {e}")
            raise
        finally:
            db.close()

    except Exception as e:
        logger.error(f"每日聚合任务失败: {e}", exc_info=True)
    finally:
        logger.info("=" * 60)


def start_scheduler():
    """启动调度器，注册定时任务"""
    # 每天早上 08:00 执行
    trigger = CronTrigger(
        hour=settings.push_hour,
        minute=settings.push_minute,
        timezone="Asia/Shanghai",
    )

    scheduler.add_job(
        daily_aggregate_job,
        trigger=trigger,
        id="daily_aggregate",
        name="每日数据聚合",
        replace_existing=True,
        misfire_grace_time=3600,  # 允许 1 小时内的延迟执行
    )

    scheduler.start()
    logger.info(
        f"定时任务已注册: 每天 {settings.push_hour:02d}:{settings.push_minute:02d} "
        f"执行每日聚合"
    )


def stop_scheduler():
    """停止调度器"""
    if scheduler.running:
        scheduler.shutdown(wait=False)
        logger.info("调度器已停止")