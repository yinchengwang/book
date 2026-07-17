"""
多模态 RAG 配置加载模块

从 YAML 配置文件加载多模态 RAG 系统配置，支持环境变量覆盖。
配置项涵盖检索、表格、图像、代码、上下文、LLM、监控等模块。
"""

import os
import json
from dataclasses import dataclass, field
from typing import Optional
from pathlib import Path


# ==================== 配置数据模型 ====================

@dataclass
class RetrievalConfig:
    """检索配置"""
    default_top_k: int = 10
    rrf_weights: dict = field(default_factory=lambda: {
        "text": 0.5,
        "table": 0.25,
        "image": 0.15,
        "code": 0.1,
    })
    rrf_k: int = 60


@dataclass
class TableConfig:
    """表格配置"""
    row_threshold: int = 10   # ≤10 行用 T1，>10 行用 T2
    max_embedding_rows: int = 3


@dataclass
class ImageConfig:
    """图像配置"""
    caption_model: str = "Salesforce/blip2-opt-2.7b"
    ocr_engine: str = "pytesseract"
    siglip_model: str = "google/siglip-so400m-patch14-384"
    image_storage_dir: str = "./storage/images"


@dataclass
class CodeConfig:
    """代码配置"""
    max_chunk_lines: int = 200
    codebert_model: str = "microsoft/graphcodebert-base"


@dataclass
class ContextConfig:
    """上下文配置"""
    max_total_tokens: int = 128000
    max_text_tokens: int = 8000
    max_table_tokens: int = 6000
    max_image_tokens: int = 4000
    max_code_tokens: int = 4000


@dataclass
class LLMConfig:
    """LLM 配置"""
    primary: str = "minimax"
    fallback: str = "claude"
    min_retry: int = 3
    timeout: int = 30
    minimax_api_key: str = ""
    minimax_model: str = "MiniMax-M3"
    claude_api_key: str = ""
    claude_model: str = "claude-sonnet-5"


@dataclass
class MonitoringConfig:
    """监控配置"""
    enable_prometheus: bool = True
    metrics_port: int = 9090
    log_slow_queries: bool = True
    slow_query_threshold: float = 2.0  # 秒


@dataclass
class RedisConfig:
    """Redis 配置"""
    host: str = "localhost"
    port: int = 6379
    db: int = 0
    stream_name: str = "rag:ingestion"
    session_ttl: int = 1800  # 30 分钟


@dataclass
class EmbeddingConfig:
    """Embedding 服务配置"""
    bge_service_url: str = "http://localhost:8001/embed/text"
    siglip_service_url: str = "http://localhost:8002/embed/image"
    codebert_service_url: str = "http://localhost:8003/embed/code"
    jina_api_key: str = ""
    jina_api_url: str = "https://api.jina.ai/v1/embeddings"
    embedding_dim: int = 1024


@dataclass
class MultimodalRAGConfig:
    """多模态 RAG 系统顶层配置"""
    retrieval: RetrievalConfig = field(default_factory=RetrievalConfig)
    table: TableConfig = field(default_factory=TableConfig)
    image: ImageConfig = field(default_factory=ImageConfig)
    code: CodeConfig = field(default_factory=CodeConfig)
    context: ContextConfig = field(default_factory=ContextConfig)
    llm: LLMConfig = field(default_factory=LLMConfig)
    monitoring: MonitoringConfig = field(default_factory=MonitoringConfig)
    redis: RedisConfig = field(default_factory=RedisConfig)
    embedding: EmbeddingConfig = field(default_factory=EmbeddingConfig)

    # VDB 库路径
    vdb_lib_path: str = "./build/libvdb.so"

    # 存储路径
    storage_dir: str = "./storage"
    upload_dir: str = "./storage/uploads"


# ==================== 配置加载 ====================

def load_config(config_path: Optional[str] = None) -> MultimodalRAGConfig:
    """
    加载多模态 RAG 配置

    优先级：环境变量 > 配置文件 > 默认值
    """
    config = MultimodalRAGConfig()

    if config_path and Path(config_path).exists():
        _load_from_yaml(config, config_path)

    _apply_env_overrides(config)

    return config


def _load_from_yaml(config: MultimodalRAGConfig, config_path: str):
    """从 YAML 文件加载配置（当前使用 JSON 替代，YAML 解析需要额外依赖）"""
    import json

    with open(config_path, "r", encoding="utf-8") as f:
        raw = json.load(f)

    rag_cfg = raw.get("rag", {})

    # 检索配置
    if "retrieval" in rag_cfg:
        rc = rag_cfg["retrieval"]
        if "default_top_k" in rc:
            config.retrieval.default_top_k = rc["default_top_k"]
        if "rrf_weights" in rc:
            config.retrieval.rrf_weights = rc["rrf_weights"]
        if "rrf_k" in rc:
            config.retrieval.rrf_k = rc["rrf_k"]

    # 表格配置
    if "table" in rag_cfg:
        tc = rag_cfg["table"]
        if "row_threshold" in tc:
            config.table.row_threshold = tc["row_threshold"]
        if "max_embedding_rows" in tc:
            config.table.max_embedding_rows = tc["max_embedding_rows"]

    # 图像配置
    if "image" in rag_cfg:
        ic = rag_cfg["image"]
        if "caption_model" in ic:
            config.image.caption_model = ic["caption_model"]
        if "ocr_engine" in ic:
            config.image.ocr_engine = ic["ocr_engine"]

    # 代码配置
    if "code" in rag_cfg:
        cc = rag_cfg["code"]
        if "max_chunk_lines" in cc:
            config.code.max_chunk_lines = cc["max_chunk_lines"]

    # 上下文配置
    if "context" in rag_cfg:
        ctx = rag_cfg["context"]
        for key in ("max_total_tokens", "max_text_tokens", "max_table_tokens",
                     "max_image_tokens", "max_code_tokens"):
            if key in ctx:
                setattr(config.context, key, ctx[key])

    # LLM 配置
    if "llm" in rag_cfg:
        lc = rag_cfg["llm"]
        if "primary" in lc:
            config.llm.primary = lc["primary"]
        if "fallback" in lc:
            config.llm.fallback = lc["fallback"]
        if "min_retry" in lc:
            config.llm.min_retry = lc["min_retry"]
        if "timeout" in lc:
            config.llm.timeout = lc["timeout"]

    # 监控配置
    if "monitoring" in rag_cfg:
        mc = rag_cfg["monitoring"]
        if "enable_prometheus" in mc:
            config.monitoring.enable_prometheus = mc["enable_prometheus"]
        if "metrics_port" in mc:
            config.monitoring.metrics_port = mc["metrics_port"]
        if "log_slow_queries" in mc:
            config.monitoring.log_slow_queries = mc["log_slow_queries"]
        if "slow_query_threshold" in mc:
            config.monitoring.slow_query_threshold = mc["slow_query_threshold"]


def _apply_env_overrides(config: MultimodalRAGConfig):
    """环境变量覆盖配置项"""
    env_map = {
        "RAG_VDB_LIB_PATH": ("vdb_lib_path", None),
        "RAG_STORAGE_DIR": ("storage_dir", None),
        "RAG_UPLOAD_DIR": ("upload_dir", None),
        "RAG_MINIMAX_API_KEY": ("llm", "minimax_api_key"),
        "RAG_CLAUDE_API_KEY": ("llm", "claude_api_key"),
        "RAG_JINA_API_KEY": ("embedding", "jina_api_key"),
        "RAG_REDIS_HOST": ("redis", "host"),
        "RAG_REDIS_PORT": ("redis", "port"),
        "RAG_BGE_SERVICE_URL": ("embedding", "bge_service_url"),
        "RAG_SIGLIP_SERVICE_URL": ("embedding", "siglip_service_url"),
        "RAG_CODEBERT_SERVICE_URL": ("embedding", "codebert_service_url"),
        "RAG_DEFAULT_TOP_K": ("retrieval", "default_top_k"),
    }

    for env_key, (attr, sub_attr) in env_map.items():
        value = os.environ.get(env_key)
        if value is not None:
            if sub_attr:
                setattr(getattr(config, attr), sub_attr, value)
            else:
                setattr(config, attr, value)


# ==================== 便捷函数 ====================

def get_config() -> MultimodalRAGConfig:
    """获取全局配置（单例模式）"""
    if not hasattr(get_config, "_instance"):
        config_path = os.environ.get("RAG_CONFIG_PATH", "config/multimodal_rag.json")
        get_config._instance = load_config(config_path)
    return get_config._instance