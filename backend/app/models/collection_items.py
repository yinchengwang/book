from sqlalchemy import Column, Integer, Text, TIMESTAMP, ForeignKey, UniqueConstraint
from app.database import Base
from datetime import datetime


class CollectionItem(Base):
    __tablename__ = "collection_items"

    id = Column(Integer, primary_key=True, autoincrement=True)
    collection_id = Column(Integer, ForeignKey("collections.id", ondelete="CASCADE"), nullable=False)
    item_id = Column(Integer, ForeignKey("items.id", ondelete="CASCADE"), nullable=False)
    note = Column(Text, default="")
    created_at = Column(TIMESTAMP, default=datetime.utcnow)

    __table_args__ = (
        UniqueConstraint("collection_id", "item_id", name="uq_collection_item"),
    )
