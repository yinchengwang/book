from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    database_url: str = "postgresql://daily_digest:daily_digest@localhost:5432/daily_digest"
    llm_api_key: str = ""
    llm_model: str = "claude-haiku-4-5-20251001"
    log_level: str = "INFO"

    # 聚合配置
    fetch_timeout: int = 30
    max_items_per_source: int = 50

    # 推送配置
    push_hour: int = 8
    push_minute: int = 0

    model_config = {"env_file": ".env", "env_file_encoding": "utf-8"}


settings = Settings()