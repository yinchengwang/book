"""
SigLIP 图像 Embedding 服务

自托管 GPU 推理服务，提供图像 Embedding 生成。
使用 SigLIP-SO400M 模型，支持批量编码和健康检查。
"""

import logging
import os
from typing import Optional

import numpy as np

logger = logging.getLogger(__name__)


class SigLIPService:
    """
    SigLIP 图像 Embedding 推理服务

    使用 Google SigLIP-SO400M 模型生成图像向量。
    输出维度: 768 (SigLIP-SO400M)。

    用法:
        service = SigLIPService(model_name="google/siglip-so400m-patch14-384")
        embedding = service.encode("/path/to/image.jpg")
    """

    def __init__(
        self,
        model_name: str = "google/siglip-so400m-patch14-384",
        device: str = "cuda",
        batch_size: int = 8,
    ):
        self.model_name = model_name
        self.device = device
        self.batch_size = batch_size
        self._model = None
        self._processor = None
        self._loaded = False

    def load(self):
        """
        加载 SigLIP 模型和处理器

        首次调用时加载，后续复用。
        """
        if self._loaded:
            return

        try:
            import torch
            from transformers import AutoModel, AutoProcessor

            logger.info(f"加载 SigLIP 模型: {self.model_name}")

            self._processor = AutoProcessor.from_pretrained(self.model_name)

            self._model = AutoModel.from_pretrained(
                self.model_name,
                torch_dtype=torch.float16 if self.device == "cuda" else torch.float32,
            )

            if self.device == "cuda" and torch.cuda.is_available():
                self._model = self._model.to(self.device)
                logger.info(f"模型已加载到 GPU: {torch.cuda.get_device_name(0)}")
            else:
                self.device = "cpu"
                self._model = self._model.to("cpu")
                logger.info("模型已加载到 CPU")

            self._model.eval()
            self._loaded = True

        except ImportError:
            logger.warning("transformers 或 torch 未安装，使用模拟模式")
            self._loaded = True  # 标记为已加载，后续使用模拟
        except Exception as e:
            logger.error(f"SigLIP 模型加载失败: {e}")
            raise

    def encode(self, image_path: str) -> np.ndarray:
        """
        编码单张图像

        Args:
            image_path: 图像文件路径

        Returns:
            Embedding 向量 (768 维)
        """
        self.load()

        if not os.path.exists(image_path):
            logger.warning(f"图像文件不存在: {image_path}")
            return np.zeros(768, dtype=np.float32)

        # 模拟模式
        if self._model is None:
            logger.info(f"[模拟] SigLIP encode: {image_path}")
            return np.random.randn(768).astype(np.float32)

        try:
            from PIL import Image
            import torch

            image = Image.open(image_path).convert("RGB")
            inputs = self._processor(images=image, return_tensors="pt")

            if self.device == "cuda":
                inputs = {k: v.to(self.device) for k, v in inputs.items()}

            with torch.no_grad():
                outputs = self._model(**inputs)

            # 取 image_embeds 或 pooler_output
            if hasattr(outputs, "image_embeds"):
                embedding = outputs.image_embeds[0].cpu().numpy()
            else:
                embedding = outputs.pooler_output[0].cpu().numpy()

            # L2 归一化
            norm = np.linalg.norm(embedding)
            if norm > 0:
                embedding = embedding / norm

            return embedding.astype(np.float32)

        except Exception as e:
            logger.error(f"图像编码失败: {image_path} - {e}")
            return np.zeros(768, dtype=np.float32)

    def encode_batch(self, image_paths: list[str]) -> np.ndarray:
        """
        批量编码图像

        Args:
            image_paths: 图像文件路径列表

        Returns:
            Embedding 矩阵 (N x 768)
        """
        self.load()

        if not image_paths:
            return np.zeros((0, 768), dtype=np.float32)

        # 模拟模式
        if self._model is None:
            return np.random.randn(len(image_paths), 768).astype(np.float32)

        embeddings = []
        for i in range(0, len(image_paths), self.batch_size):
            batch = image_paths[i:i + self.batch_size]
            batch_embs = [self.encode(p) for p in batch]
            embeddings.extend(batch_embs)

        return np.array(embeddings, dtype=np.float32)

    @staticmethod
    def check_gpu_available() -> bool:
        """
        检查 GPU 是否可用

        Returns:
            GPU 可用返回 True
        """
        try:
            import torch
            return torch.cuda.is_available()
        except ImportError:
            return False

    def health_check(self) -> dict:
        """
        健康检查

        Returns:
            {"status": "ok"|"error", "gpu_available": bool, "model_loaded": bool}
        """
        gpu_available = self.check_gpu_available()

        if self._loaded and self._model is not None:
            return {
                "status": "ok",
                "gpu_available": gpu_available,
                "model_loaded": True,
                "device": self.device,
                "model": self.model_name,
            }
        elif self._loaded:
            return {
                "status": "degraded",
                "gpu_available": gpu_available,
                "model_loaded": False,
                "note": "模拟模式",
            }
        else:
            return {
                "status": "not_loaded",
                "gpu_available": gpu_available,
                "model_loaded": False,
            }