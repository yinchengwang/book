"""
GraphCodeBERT 代码 Embedding 服务

自托管 GPU 推理服务，提供代码片段 Embedding 生成。
使用 Microsoft GraphCodeBERT 模型。
"""

import logging
from typing import Optional

import numpy as np

logger = logging.getLogger(__name__)


class CodeBERTService:
    """
    GraphCodeBERT 代码 Embedding 服务

    使用 Microsoft GraphCodeBERT 模型生成代码向量。
    输出维度: 768。

    用法:
        service = CodeBERTService()
        embedding = service.encode("def hello(): pass", "python")
    """

    def __init__(
        self,
        model_name: str = "microsoft/graphcodebert-base",
        device: str = "cuda",
    ):
        self.model_name = model_name
        self.device = device
        self._model = None
        self._tokenizer = None
        self._loaded = False

    def load(self):
        """加载 GraphCodeBERT 模型和 tokenizer"""
        if self._loaded:
            return

        try:
            import torch
            from transformers import AutoModel, AutoTokenizer

            logger.info(f"加载 GraphCodeBERT 模型: {self.model_name}")

            self._tokenizer = AutoTokenizer.from_pretrained(self.model_name)
            self._model = AutoModel.from_pretrained(self.model_name)

            if self.device == "cuda" and torch.cuda.is_available():
                self._model = self._model.to(self.device)
                logger.info("模型已加载到 GPU")
            else:
                self.device = "cpu"
                logger.info("模型已加载到 CPU")

            self._model.eval()
            self._loaded = True

        except ImportError:
            logger.warning("transformers 或 torch 未安装，使用模拟模式")
            self._loaded = True
        except Exception as e:
            logger.error(f"GraphCodeBERT 加载失败: {e}")
            raise

    def encode(self, code: str, language: str = "python") -> np.ndarray:
        """
        编码代码片段

        Args:
            code: 代码文本
            language: 编程语言

        Returns:
            Embedding 向量 (768 维)
        """
        self.load()

        if self._model is None:
            logger.info(f"[模拟] CodeBERT encode: {language}")
            return np.random.randn(768).astype(np.float32)

        try:
            import torch

            inputs = self._tokenizer(
                code,
                return_tensors="pt",
                truncation=True,
                max_length=512,
                padding="max_length",
            )

            if self.device == "cuda":
                inputs = {k: v.to(self.device) for k, v in inputs.items()}

            with torch.no_grad():
                outputs = self._model(**inputs)

            # 取 [CLS] token 的表示
            embedding = outputs.last_hidden_state[:, 0, :].cpu().numpy()[0]

            # L2 归一化
            norm = np.linalg.norm(embedding)
            if norm > 0:
                embedding = embedding / norm

            return embedding.astype(np.float32)

        except Exception as e:
            logger.error(f"代码编码失败: {e}")
            return np.zeros(768, dtype=np.float32)