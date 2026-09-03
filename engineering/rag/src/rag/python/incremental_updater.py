"""
增量更新模块

支持多模态 chunk 的版本对比 + diff，实现增量更新。
"""

import hashlib
import json
import logging
from typing import Optional

logger = logging.getLogger(__name__)


def compute_content_hash(chunk) -> str:
    """
    多模态 chunk 的 content hash 计算

    不同模态使用不同的 signing 策略：
    - 表格: headers + rows 完整内容
    - 图像: 图片内容 hash + caption hash
    - 代码: 函数签名 + 内容
    - 文本: 直接 hash 内容

    Args:
        chunk: chunk 对象（TextChunk / TableChunk / ImageChunk / CodeChunk）

    Returns:
        SHA256 hex 字符串
    """
    chunk_type = getattr(chunk, "chunk_type", "text") or getattr(chunk, "type", "text")

    if chunk_type in ("table", "T1", "T2"):
        # 表格: headers + rows 完整内容
        headers = getattr(chunk, "headers", []) or []
        rows = getattr(chunk, "rows", []) or []
        title = getattr(chunk, "title", "") or ""
        signing_content = json.dumps({
            "headers": headers,
            "rows": rows,
            "title": title,
        }, sort_keys=True, ensure_ascii=False)

    elif chunk_type in ("image",):
        # 图像: 图片内容 hash + caption hash
        image_path = getattr(chunk, "image_path", "")
        caption = getattr(chunk, "caption", "") or ""

        image_hash = ""
        try:
            with open(image_path, "rb") as f:
                image_hash = hashlib.sha256(f.read()).hexdigest()
        except (FileNotFoundError, IOError) as e:
            logger.warning(f"图像文件无法读取: {image_path} - {e}")
            image_hash = "file_not_found"

        caption_hash = hashlib.sha256(caption.encode()).hexdigest()
        signing_content = image_hash + caption_hash

    elif chunk_type in ("code",):
        # 代码: 语言 + 名称 + 内容
        language = getattr(chunk, "language", "") or ""
        name = getattr(chunk, "name", "") or ""
        code = getattr(chunk, "code", "") or getattr(chunk, "content", "") or ""
        signing_content = json.dumps({
            "language": language,
            "name": name,
            "content": code,
        }, sort_keys=True, ensure_ascii=False)

    else:
        # 文本: 直接 hash 内容
        content = getattr(chunk, "content", "") or ""
        signing_content = content

    return hashlib.sha256(signing_content.encode("utf-8")).hexdigest()


class UpdateResult:
    """增量更新结果"""
    def __init__(self, added: int = 0, removed: int = 0, updated: int = 0):
        self.added = added
        self.removed = removed
        self.updated = updated


class IncrementalUpdater:
    """
    增量更新器

    版本对比 + diff，支持多模态 chunk 的增量更新。

    用法:
        updater = IncrementalUpdater(db_api)
        result = await updater.update(doc_id, tenant_id, new_file_path, parse_fn, chunk_fn)
    """

    def __init__(self, db_api=None):
        self.db_api = db_api

    async def update(
        self,
        doc_id: int,
        tenant_id: int,
        new_file_path: str,
        parse_fn=None,
        chunk_fn=None,
    ) -> UpdateResult:
        """
        执行增量更新

        Args:
            doc_id: 文档 ID
            tenant_id: 租户 ID
            new_file_path: 新文档路径
            parse_fn: 文档解析函数
            chunk_fn: 切分函数

        Returns:
            UpdateResult
        """
        if not parse_fn or not chunk_fn:
            logger.warning("parse_fn 或 chunk_fn 未提供，跳过增量更新")
            return UpdateResult()

        # 1. 解析新文档
        logger.info(f"解析新文档: {new_file_path}")
        new_elements = await parse_fn(new_file_path)
        new_chunks = await chunk_fn(new_elements)

        # 2. 新 chunk 计算 hash
        new_chunk_map = {}
        for chunk in new_chunks:
            chunk.content_hash = compute_content_hash(chunk)
            new_chunk_map[chunk.content_hash] = chunk

        # 3. 查询旧 chunk
        old_chunks = await self._get_old_chunks(doc_id, tenant_id)

        if not old_chunks:
            # 新文档: 直接插入
            await self._batch_insert(new_chunks)
            logger.info(f"新文档，插入 {len(new_chunks)} 个 chunk")
            return UpdateResult(added=len(new_chunks))

        # 4. 旧 chunk hash 映射
        old_hash_map = {c.get("content_hash", ""): c for c in old_chunks}
        old_id_map = {c.get("chunk_id", 0): c for c in old_chunks}

        # 5. 计算 diff
        added = []
        removed = []
        updated = []

        for h, new_chunk in new_chunk_map.items():
            if h not in old_hash_map:
                added.append(new_chunk)
            else:
                # 内容没变，检查 metadata 是否更新
                old = old_hash_map[h]
                if getattr(new_chunk, "metadata", {}) != old.get("metadata", {}):
                    updated.append((new_chunk, old.get("chunk_id", 0)))

        for old_chunk in old_chunks:
            if old_chunk.get("content_hash", "") not in new_chunk_map:
                removed.append(old_chunk)

        # 6. 执行批量操作
        if removed:
            removed_ids = [c.get("chunk_id", 0) for c in removed if c.get("chunk_id")]
            await self._batch_delete(removed_ids)

        if added:
            await self._batch_insert(added)

        if updated:
            for new_chunk, old_id in updated:
                await self._update_chunk(old_id, new_chunk)

        result = UpdateResult(
            added=len(added),
            removed=len(removed),
            updated=len(updated),
        )

        logger.info(f"增量更新完成: +{result.added} -{result.removed} ~{result.updated}")
        return result

    async def _get_old_chunks(self, doc_id: int, tenant_id: int) -> list[dict]:
        """查询旧 chunk（从 VDB 存储）"""
        if self.db_api:
            # TODO: 实际查询
            pass
        return []

    async def _batch_insert(self, chunks: list):
        """批量插入"""
        if self.db_api:
            for chunk in chunks:
                pass  # TODO: 调用 db_api.insert_*_chunk()
        else:
            logger.info(f"[模拟] 批量插入 {len(chunks)} 个 chunk")

    async def _batch_delete(self, chunk_ids: list[int]):
        """批量删除"""
        if self.db_api:
            pass  # TODO: 调用 db_api.delete_chunk()
        else:
            logger.info(f"[模拟] 批量删除 {len(chunk_ids)} 个 chunk")

    async def _update_chunk(self, chunk_id: int, new_chunk):
        """更新单个 chunk"""
        if self.db_api:
            pass  # TODO: 调用 db_api.update_chunk()
        else:
            logger.info(f"[模拟] 更新 chunk {chunk_id}")

    @staticmethod
    def _fuzzy_match(old_chunks: list[dict], new_chunks: list) -> list[tuple]:
        """
        模糊匹配更新 chunk

        当 content_hash 不匹配但内容高度相似时，使用 fuzzy match 检测更新。

        Args:
            old_chunks: 旧 chunk 列表
            new_chunks: 新 chunk 列表

        Returns:
            [(old_chunk, new_chunk), ...] 匹配到的更新对
        """
        updates = []
        for new_chunk in new_chunks:
            new_content = getattr(new_chunk, "content", "") or ""
            if not new_content:
                continue

            best_match = None
            best_score = 0.0

            for old_chunk in old_chunks:
                old_content = old_chunk.get("content", "")
                if not old_content:
                    continue

                # 简单相似度：计算公共子序列比例
                score = IncrementalUpdater._text_similarity(new_content, old_content)
                if score > 0.8 and score > best_score:  # 80% 以上相似
                    best_match = old_chunk
                    best_score = score

            if best_match:
                updates.append((best_match, new_chunk))

        return updates

    @staticmethod
    def _text_similarity(text1: str, text2: str) -> float:
        """
        计算两段文本的相似度

        使用最长公共子序列 (LCS) 的简化版本。

        Args:
            text1: 文本 1
            text2: 文本 2

        Returns:
            相似度 (0.0 - 1.0)
        """
        if not text1 or not text2:
            return 0.0

        # 使用 set intersection 的简化方法
        set1 = set(text1.split())
        set2 = set(text2.split())

        if not set1 or not set2:
            return 0.0

        intersection = set1 & set2
        union = set1 | set2

        return len(intersection) / len(union)