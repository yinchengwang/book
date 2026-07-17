"""
图像切分模块 — I2 + I1 Fallback 策略

I2（主路）：SigLIP 向量检索
I1（Fallback）：OCR (pytesseract) + BLIP caption → 文本向量检索
"""

import logging
import os
from typing import Optional

import numpy as np

logger = logging.getLogger(__name__)


class ImageChunk:
    """图像块输出"""
    def __init__(
        self,
        image_path: str = "",
        siglip_embedding: Optional[np.ndarray] = None,
        text_embedding: Optional[np.ndarray] = None,
        ocr_text: Optional[str] = None,
        caption: Optional[str] = None,
        metadata: dict = None,
    ):
        self.image_path = image_path
        self.siglip_embedding = siglip_embedding
        self.text_embedding = text_embedding
        self.ocr_text = ocr_text
        self.caption = caption
        self.metadata = metadata or {}


class ImageChunker:
    """
    图像切分器

    I2（主路）：SigLIP 向量检索
    I1（Fallback）：OCR + Caption → 文本向量检索

    用法:
        chunker = ImageChunker(siglip_service, ocr_engine, caption_model)
        result = chunker.chunk("/path/to/image.jpg", {"page_num": 3})
    """

    def __init__(self, siglip_service=None, ocr_engine=None, caption_model=None):
        self.siglip = siglip_service
        self.ocr = ocr_engine
        self.caption = caption_model

    def chunk(self, image_path: str, metadata: dict = None) -> ImageChunk:
        """
        处理单张图像

        流程:
        1. SigLIP 向量（主路）
        2. OCR 提取文字（fallback + 补充）
        3. Caption 生成
        4. 组合 embedding（图像 + OCR + Caption）

        Args:
            image_path: 图像文件路径
            metadata: 附加元数据

        Returns:
            ImageChunk 对象
        """
        if metadata is None:
            metadata = {}

        if not os.path.exists(image_path):
            logger.warning(f"图像文件不存在: {image_path}")
            return ImageChunk(
                image_path=image_path,
                metadata={**metadata, "error": "file_not_found"},
            )

        # 1. SigLIP 向量（主路）
        siglip_embedding = None
        if self.siglip:
            try:
                siglip_embedding = self.siglip.encode(image_path)
                logger.info(f"SigLIP 编码成功: {image_path}")
            except Exception as e:
                logger.warning(f"SigLIP 编码失败: {image_path} - {e}")

        # 2. OCR 提取文字（fallback + 补充）
        ocr_text = None
        if self.ocr:
            try:
                ocr_text = self._extract_ocr(image_path)
                logger.info(f"OCR 提取成功: {image_path} ({len(ocr_text)} chars)")
            except Exception as e:
                logger.warning(f"OCR 提取失败: {image_path} - {e}")

        # 3. Caption 生成
        caption = None
        if self.caption:
            try:
                caption = self._generate_caption(image_path)
                logger.info(f"Caption 生成成功: {image_path}")
            except Exception as e:
                logger.warning(f"Caption 生成失败: {image_path} - {e}")

        # 4. 组合文本内容用于文本向量召回
        combined_content = self._combine_content(ocr_text, caption)
        text_embedding = None
        if combined_content:
            # TODO: 调用 BGE embedding 服务
            pass

        return ImageChunk(
            image_path=image_path,
            siglip_embedding=siglip_embedding,
            text_embedding=text_embedding,
            ocr_text=ocr_text,
            caption=caption,
            metadata={
                **metadata,
                "has_ocr": ocr_text is not None,
                "has_caption": caption is not None,
                "has_siglip": siglip_embedding is not None,
            },
        )

    def _extract_ocr(self, image_path: str) -> Optional[str]:
        """
        使用 pytesseract 提取图像中的文字

        Args:
            image_path: 图像文件路径

        Returns:
            提取的文字内容，失败返回 None
        """
        try:
            import pytesseract
            from PIL import Image

            image = Image.open(image_path).convert("RGB")
            text = pytesseract.image_to_string(image, lang="chi_sim+eng")

            # 清理空白字符
            text = text.strip()
            return text if text else None

        except ImportError:
            logger.warning("pytesseract 未安装，跳过 OCR")
            return None
        except Exception as e:
            logger.error(f"OCR 处理失败: {e}")
            return None

    def _generate_caption(self, image_path: str) -> Optional[str]:
        """
        使用 BLIP2 生成图像描述

        Args:
            image_path: 图像文件路径

        Returns:
            生成的 caption，失败返回 None
        """
        try:
            from transformers import Blip2Processor, Blip2ForConditionalGeneration
            import torch
            from PIL import Image

            # 使用 BLIP2-OPT-2.7B
            processor = Blip2Processor.from_pretrained("Salesforce/blip2-opt-2.7b")
            model = Blip2ForConditionalGeneration.from_pretrained(
                "Salesforce/blip2-opt-2.7b",
                torch_dtype=torch.float16,
            )

            if torch.cuda.is_available():
                model = model.to("cuda")

            image = Image.open(image_path).convert("RGB")
            inputs = processor(images=image, return_tensors="pt")

            if torch.cuda.is_available():
                inputs = {k: v.to("cuda") for k, v in inputs.items()}

            with torch.no_grad():
                generated_ids = model.generate(**inputs, max_new_tokens=50)

            caption = processor.decode(generated_ids[0], skip_special_tokens=True)
            return caption.strip() if caption else None

        except ImportError:
            logger.warning("transformers 或 torch 未安装，跳过 Caption 生成")
            return None
        except Exception as e:
            logger.error(f"Caption 生成失败: {e}")
            return None

    @staticmethod
    def _combine_content(ocr_text: Optional[str], caption: Optional[str]) -> Optional[str]:
        """
        组合 OCR 文字和 Caption 用于文本向量召回

        Args:
            ocr_text: OCR 提取的文字
            caption: 图像描述

        Returns:
            组合后的文本，用于文本向量召回
        """
        parts = []
        if caption:
            parts.append(f"图像描述: {caption}")
        if ocr_text:
            parts.append(f"图像文字: {ocr_text}")
        return "\n".join(parts) if parts else None