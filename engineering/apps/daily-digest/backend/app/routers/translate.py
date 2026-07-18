import logging
from pydantic import BaseModel
from fastapi import APIRouter
from app.services.processor.summarizer import Summarizer

logger = logging.getLogger("router.translate")
router = APIRouter(tags=["translate"])


class TranslateRequest(BaseModel):
    """翻译请求模型"""
    text: str
    source_lang: str = "en"
    target_lang: str = "zh"


class TranslateResponse(BaseModel):
    """翻译响应模型"""
    translated_text: str


@router.post("/api/v1/translate", response_model=TranslateResponse)
async def translate(req: TranslateRequest):
    """调用 LLM 翻译英文技术内容为中文"""
    summarizer = Summarizer()
    prompt = f"请将以下英文技术内容翻译成中文，保持专业术语准确，不要添加解释：\n\n{req.text}"
    try:
        # 根据 model 选择调用 Claude 或 OpenAI
        if "claude" in summarizer.model.lower():
            translated = await summarizer._call_claude(prompt)
        else:
            translated = await summarizer._call_openai(prompt)
        if not translated:
            translated = req.text  # fallback 返回原文
    except Exception as e:
        logger.error(f"翻译失败: {e}")
        translated = req.text  # 异常时返回原文
    return TranslateResponse(translated_text=translated)