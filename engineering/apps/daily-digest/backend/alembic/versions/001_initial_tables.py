"""初始建表

Revision ID: 001
Revises:
Create Date: 2026-07-18
"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers
revision: str = "001"
down_revision: Union[str, None] = None
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    # items — 聚合内容
    op.create_table(
        "items",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("source", sa.String(32), nullable=False, index=True),
        sa.Column("source_id", sa.String(128), nullable=False),
        sa.Column("title", sa.Text(), nullable=False),
        sa.Column("url", sa.Text(), nullable=False),
        sa.Column("summary", sa.Text(), server_default=""),
        sa.Column("raw_content", sa.Text(), server_default=""),
        sa.Column("category", sa.String(32), server_default="other", index=True),
        sa.Column("tags", sa.JSON(), server_default="[]"),
        sa.Column("score", sa.Float(), server_default="0.0"),
        sa.Column("published", sa.TIMESTAMP(), nullable=True),
        sa.Column("source_weight", sa.Integer(), server_default="0"),
        sa.Column("created_at", sa.TIMESTAMP(), server_default=sa.func.now()),
        sa.Column("is_push", sa.Boolean(), server_default="false"),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("source", "source_id", name="uq_item_source_id"),
    )

    # users — 用户
    op.create_table(
        "users",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("wechat_openid", sa.String(64), nullable=True, unique=True),
        sa.Column("email", sa.String(255), nullable=True, unique=True),
        sa.Column("preferences", sa.Text(), server_default="{}"),
        sa.Column("created_at", sa.TIMESTAMP(), server_default=sa.func.now()),
        sa.Column("last_active", sa.TIMESTAMP(), server_default=sa.func.now()),
        sa.PrimaryKeyConstraint("id"),
    )

    # subscriptions — 用户订阅方向
    op.create_table(
        "subscriptions",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("user_id", sa.Integer(), sa.ForeignKey("users.id", ondelete="CASCADE"), nullable=False),
        sa.Column("category", sa.String(32), nullable=False),
        sa.Column("keywords", sa.JSON(), server_default="[]"),
        sa.Column("weight", sa.Integer(), server_default="0"),
        sa.Column("created_at", sa.TIMESTAMP(), server_default=sa.func.now()),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("user_id", "category", name="uq_user_category"),
    )

    # user_actions — 用户行为
    op.create_table(
        "user_actions",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("user_id", sa.Integer(), sa.ForeignKey("users.id", ondelete="CASCADE"), nullable=False),
        sa.Column("item_id", sa.Integer(), sa.ForeignKey("items.id", ondelete="CASCADE"), nullable=False),
        sa.Column("action", sa.String(16), nullable=False),
        sa.Column("created_at", sa.TIMESTAMP(), server_default=sa.func.now()),
        sa.PrimaryKeyConstraint("id"),
    )
    op.create_index("ix_user_actions_user_id", "user_actions", ["user_id"])
    op.create_index("ix_user_actions_item_id", "user_actions", ["item_id"])

    # collections — 收藏夹
    op.create_table(
        "collections",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("user_id", sa.Integer(), sa.ForeignKey("users.id", ondelete="CASCADE"), nullable=False),
        sa.Column("name", sa.String(64), nullable=False),
        sa.Column("description", sa.Text(), server_default=""),
        sa.Column("created_at", sa.TIMESTAMP(), server_default=sa.func.now()),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("user_id", "name", name="uq_user_collection_name"),
    )

    # collection_items — 收藏夹内容
    op.create_table(
        "collection_items",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("collection_id", sa.Integer(), sa.ForeignKey("collections.id", ondelete="CASCADE"), nullable=False),
        sa.Column("item_id", sa.Integer(), sa.ForeignKey("items.id", ondelete="CASCADE"), nullable=False),
        sa.Column("note", sa.Text(), server_default=""),
        sa.Column("created_at", sa.TIMESTAMP(), server_default=sa.func.now()),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("collection_id", "item_id", name="uq_collection_item"),
    )


def downgrade() -> None:
    op.drop_table("collection_items")
    op.drop_table("collections")
    op.drop_table("user_actions")
    op.drop_table("subscriptions")
    op.drop_table("users")
    op.drop_table("items")
