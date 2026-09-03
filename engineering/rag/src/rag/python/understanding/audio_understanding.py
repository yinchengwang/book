"""
@file audio_understanding.py
@brief 音频理解与检索服务

功能:
1. 音频 → Whisper 转录
2. 转录文本 → 段落分割
3. 段落 → Text Embedding
4. 支持时间戳索引
"""

import asyncio
import os
from dataclasses import dataclass, field
from typing import List, Dict, Any, Optional, Tuple
import numpy as np

try:
    import whisper
    WHISPER_AVAILABLE = True
except ImportError:
    WHISPER_AVAILABLE = False


@dataclass
class AudioSegment:
    """音频片段"""
    segment_id: str
    start_time: float  # 秒
    end_time: float    # 秒
    duration: float = 0.0

    # 内容
    text: str = ""
    embedding: Optional[np.ndarray] = None

    # 说话人信息
    speaker: Optional[str] = None
    speaker_confidence: float = 0.0

    # 音频质量
    volume_db: float = 0.0
    snr: float = 0.0  # 信噪比

    # 元数据
    language: str = "en"
    words: List[Dict[str, Any]] = field(default_factory=list)  # 词级时间戳

    def to_dict(self) -> Dict[str, Any]:
        return {
            "segment_id": self.segment_id,
            "start_time": self.start_time,
            "end_time": self.end_time,
            "duration": self.duration,
            "text": self.text,
            "speaker": self.speaker,
            "language": self.language,
        }


@dataclass
class AudioIndex:
    """音频索引"""
    audio_path: str
    duration: float = 0.0
    sample_rate: int = 16000
    channels: int = 1

    segments: List[AudioSegment] = field(default_factory=list)
    embeddings: Optional[np.ndarray] = None  # [N, D]

    # 全文检索
    full_transcript: str = ""

    metadata: Dict[str, Any] = field(default_factory=dict)

    def get_segment_by_time(self, timestamp: float) -> Optional[AudioSegment]:
        """根据时间戳获取片段"""
        for seg in self.segments:
            if seg.start_time <= timestamp <= seg.end_time:
                return seg
        return None

    def search_segments(
        self,
        query_embedding: np.ndarray,
        top_k: int = 5
    ) -> List[Tuple[AudioSegment, float]]:
        """根据 embedding 搜索片段"""
        if self.embeddings is None:
            return []

        similarities = np.dot(self.embeddings, query_embedding)
        top_indices = np.argsort(similarities)[-top_k:][::-1]

        return [
            (self.segments[i], float(similarities[i]))
            for i in top_indices if i < len(self.segments)
        ]

    def to_dict(self) -> Dict[str, Any]:
        return {
            "audio_path": self.audio_path,
            "duration": self.duration,
            "segment_count": len(self.segments),
            "segments": [s.to_dict() for s in self.segments],
            "full_transcript": self.full_transcript[:500] + "..." if len(self.full_transcript) > 500 else self.full_transcript,
        }


@dataclass
class AudioRetrievalResult:
    """音频检索结果"""
    segment: AudioSegment
    similarity: float
    timestamp: str
    snippet: str


class AudioUnderstandingService:
    """
    音频理解与检索服务

    处理流程:
    1. Whisper 转录 (带时间戳)
    2. 段落分割
    3. 文本 embedding
    4. 构建索引
    """

    def __init__(
        self,
        whisper_model: str = "large-v3",
        use_cuda: bool = True,
        language: Optional[str] = None
    ):
        """
        @param whisper_model: Whisper 模型大小 (tiny/base/small/medium/large)
        @param use_cuda: 是否使用 GPU
        @param language: 指定语言 (None 为自动检测)
        """
        self.whisper_model_name = whisper_model
        self.use_cuda = use_cuda
        self.language = language

        # 模型实例
        self._whisper_model = None

    @property
    def whisper(self):
        """延迟加载 Whisper 模型"""
        if self._whisper_model is None:
            if WHISPER_AVAILABLE:
                self._whisper_model = whisper.load_model(
                    self.whisper_model_name,
                    device="cuda" if self.use_cuda else "cpu"
                )
            else:
                raise RuntimeError("Whisper not available. Install with: pip install openai-whisper")
        return self._whisper_model

    async def process_audio(
        self,
        audio_path: str,
        paragraph_gap: float = 2.0,  # 段落间沉默阈值 (秒)
        min_segment_duration: float = 1.0,  # 最小片段时长
        max_segment_duration: float = 30.0,  # 最大片段时长
    ) -> AudioIndex:
        """
        处理音频

        @param audio_path: 音频路径
        @param paragraph_gap: 段落间沉默阈值
        @param min_segment_duration: 最小片段时长
        @param max_segment_duration: 最大片段时长
        @return 音频索引
        """
        # 1. 获取音频元数据
        metadata = self._get_audio_metadata(audio_path)

        # 2. 转录
        transcript_result = await self._transcribe(audio_path)

        # 3. 段落分割
        segments = self._segment_transcript(
            transcript_result,
            paragraph_gap,
            min_segment_duration,
            max_segment_duration
        )

        # 4. 文本 embedding
        texts = [seg.text for seg in segments]
        embeddings = await self._encode_text(texts)

        # 5. 构建索引
        full_transcript = " ".join(seg.text for seg in segments)

        return AudioIndex(
            audio_path=audio_path,
            duration=metadata["duration"],
            sample_rate=metadata["sample_rate"],
            channels=metadata["channels"],
            segments=segments,
            embeddings=embeddings,
            full_transcript=full_transcript,
            metadata=metadata,
        )

    def _get_audio_metadata(self, audio_path: str) -> Dict[str, Any]:
        """获取音频元数据"""
        metadata = {
            "duration": 0.0,
            "sample_rate": 16000,
            "channels": 1,
        }

        if not os.path.exists(audio_path):
            return metadata

        try:
            import soundfile as sf
            info = sf.info(audio_path)
            metadata["duration"] = info.duration
            metadata["sample_rate"] = info.samplerate
            metadata["channels"] = info.channels
        except Exception:
            # 降级: 使用 whisper 检测
            pass

        return metadata

    async def _transcribe(
        self,
        audio_path: str,
        word_timestamps: bool = True
    ) -> Dict[str, Any]:
        """
        使用 Whisper 转录

        @return {
            "text": str,  # 完整文本
            "segments": [{"start": float, "end": float, "text": str, "words": [...]}],
            "language": str
        }
        """
        if not WHISPER_AVAILABLE:
            return {"text": "", "segments": [], "language": "en"}

        # Whisper 推理 (同步, 但我们包装为 async)
        loop = asyncio.get_event_loop()
        result = await loop.run_in_executor(
            None,
            lambda: self.whisper.transcribe(
                audio_path,
                word_timestamps=word_timestamps,
                language=self.language,
                condition_on_previous_text=False,
            )
        )

        return {
            "text": result["text"],
            "segments": result["segments"],
            "language": result.get("language", "en"),
        }

    def _segment_transcript(
        self,
        transcript: Dict[str, Any],
        paragraph_gap: float,
        min_duration: float,
        max_duration: float
    ) -> List[AudioSegment]:
        """将转录结果分割为段落"""
        segments = []
        transcript_segments = transcript.get("segments", [])

        if not transcript_segments:
            return segments

        current_text = ""
        current_start = transcript_segments[0]["start"]
        current_end = 0.0
        current_words = []

        for i, seg in enumerate(transcript_segments):
            seg_text = seg["text"].strip()
            seg_start = seg["start"]
            seg_end = seg["end"]

            # 收集词级时间戳
            seg_words = seg.get("words", [])
            current_words.extend(seg_words)

            # 检查是否需要开始新段落
            is_new_paragraph = False

            if i > 0:
                prev_seg = transcript_segments[i - 1]
                gap = seg_start - prev_seg["end"]

                # 大间隔 = 新段落
                if gap > paragraph_gap:
                    is_new_paragraph = True

                # 超过最大时长 = 强制分割
                elif seg_end - current_start > max_duration:
                    is_new_paragraph = True

            # 开始新段落
            if is_new_paragraph and current_text:
                duration = current_end - current_start

                if duration >= min_duration:
                    segments.append(AudioSegment(
                        segment_id=f"seg_{len(segments):04d}",
                        start_time=current_start,
                        end_time=current_end,
                        duration=duration,
                        text=current_text.strip(),
                        words=current_words,
                        language=transcript.get("language", "en"),
                    ))

                current_text = ""
                current_start = seg_start
                current_words = []

            # 追加当前片段
            current_text += " " + seg_text
            current_end = seg_end

        # 添加最后一个段落
        if current_text.strip():
            duration = current_end - current_start
            if duration >= min_duration:
                segments.append(AudioSegment(
                    segment_id=f"seg_{len(segments):04d}",
                    start_time=current_start,
                    end_time=current_end,
                    duration=duration,
                    text=current_text.strip(),
                    words=current_words,
                    language=transcript.get("language", "en"),
                ))

        return segments

    async def _encode_text(self, texts: List[str]) -> Optional[np.ndarray]:
        """编码文本"""
        if not texts:
            return None

        # TODO: 使用文本 embedding 模型
        # from rag.embedding import get_embedding_model
        # model = get_embedding_model()
        # return model.encode(texts)

        return None

    async def retrieve_audio(
        self,
        query: str,
        query_embedding: np.ndarray,
        audio_index: AudioIndex,
        top_k: int = 5
    ) -> List[AudioRetrievalResult]:
        """
        检索音频片段

        @param query: 查询文本
        @param query_embedding: 查询 embedding
        @param audio_index: 音频索引
        @param top_k: 返回结果数
        @return 检索结果
        """
        if audio_index.embeddings is None:
            # 无 embedding，基于文本匹配
            return self._text_search(query, audio_index, top_k)

        # 基于 embedding 检索
        results = []
        found_segments = audio_index.search_segments(query_embedding, top_k)

        for seg, sim in found_segments:
            results.append(AudioRetrievalResult(
                segment=seg,
                similarity=sim,
                timestamp=f"{seg.start_time:.1f}s - {seg.end_time:.1f}s",
                snippet=seg.text[:200] + "..." if len(seg.text) > 200 else seg.text,
            ))

        return results

    def _text_search(
        self,
        query: str,
        audio_index: AudioIndex,
        top_k: int
    ) -> List[AudioRetrievalResult]:
        """基于文本的简单检索"""
        results = []
        query_lower = query.lower()

        for seg in audio_index.segments:
            # 简单的词匹配
            seg_words = set(seg.text.lower().split())
            query_words = set(query_lower.split())

            overlap = len(seg_words & query_words)
            if overlap > 0:
                similarity = overlap / max(len(query_words), 1)
                results.append(AudioRetrievalResult(
                    segment=seg,
                    similarity=float(similarity),
                    timestamp=f"{seg.start_time:.1f}s - {seg.end_time:.1f}s",
                    snippet=seg.text[:200],
                ))

        # 排序
        results.sort(key=lambda x: x.similarity, reverse=True)
        return results[:top_k]

    async def speaker_diarization(
        self,
        audio_path: str
    ) -> List[Dict[str, Any]]:
        """
        说话人分离

        @return [{"start": float, "end": float, "speaker": str}]
        """
        # TODO: 使用 pyannote 或 similar 进行说话人分离
        return []

    async def generate_summary(
        self,
        audio_index: AudioIndex,
        max_length: int = 500
    ) -> str:
        """生成音频摘要"""
        if not audio_index.full_transcript:
            return "音频内容无法转录"

        # TODO: 使用 LLM 生成摘要
        # from rag.llm import get_llm
        # llm = get_llm()
        # summary = llm.summarize(audio_index.full_transcript, max_length)

        return audio_index.full_transcript[:max_length]


# ========== Audio Chunker ==========

class AudioChunker:
    """
    音频分块器

    将音频文件分割为适合检索的片段
    """

    def __init__(
        self,
        chunk_duration: float = 30.0,
        overlap: float = 2.0,
        min_duration: float = 5.0
    ):
        """
        @param chunk_duration: 目标块时长 (秒)
        @param overlap: 重叠时长 (秒)
        @param min_duration: 最小块时长 (秒)
        """
        self.chunk_duration = chunk_duration
        self.overlap = overlap
        self.min_duration = min_duration

    def chunk_audio(
        self,
        audio_duration: float,
        transcript_segments: List[Dict[str, Any]]
    ) -> List[AudioSegment]:
        """
        根据转录结果分块

        @param audio_duration: 音频总时长
        @param transcript_segments: Whisper 转录的片段
        @return 音频片段列表
        """
        if not transcript_segments:
            # 无转录，按时间均分
            return self._chunk_by_duration(audio_duration)

        # 按语义分块
        return self._chunk_by_semantics(transcript_segments)

    def _chunk_by_duration(self, audio_duration: float) -> List[AudioSegment]:
        """按固定时长分块"""
        chunks = []
        start = 0.0

        while start < audio_duration:
            end = min(start + self.chunk_duration, audio_duration)
            duration = end - start

            if duration >= self.min_duration:
                chunks.append(AudioSegment(
                    segment_id=f"chunk_{len(chunks):04d}",
                    start_time=start,
                    end_time=end,
                    duration=duration,
                ))

            start = end - self.overlap
            if start < 0:
                start = end

        return chunks

    def _chunk_by_semantics(
        self,
        transcript_segments: List[Dict[str, Any]]
    ) -> List[AudioSegment]:
        """按语义分块 (基于转录的自然停顿)"""
        chunks = []
        current_start = 0.0
        current_end = 0.0
        current_text = ""

        for seg in transcript_segments:
            seg_text = seg["text"].strip()
            seg_start = seg["start"]
            seg_end = seg["end"]
            seg_duration = seg_end - seg_start

            # 检查是否需要分割
            should_split = False

            if current_text:
                total_duration = seg_end - current_start

                # 超时
                if total_duration >= self.chunk_duration:
                    should_split = True

                # 语义边界 (句子结束)
                elif seg_text.endswith(('.', '!', '?', '。', '！', '？')):
                    if total_duration >= self.min_duration:
                        should_split = True

            if should_split and current_text:
                chunks.append(AudioSegment(
                    segment_id=f"chunk_{len(chunks):04d}",
                    start_time=current_start,
                    end_time=current_end,
                    duration=current_end - current_start,
                    text=current_text.strip(),
                ))
                current_start = seg_start
                current_text = ""

            current_text += " " + seg_text
            current_end = seg_end

        # 添加最后一个块
        if current_text.strip():
            chunks.append(AudioSegment(
                segment_id=f"chunk_{len(chunks):04d}",
                start_time=current_start,
                end_time=current_end,
                duration=current_end - current_start,
                text=current_text.strip(),
            ))

        return chunks


# ========== Factory ==========

def create_audio_understanding_service(
    whisper_model: str = "large-v3",
    use_cuda: bool = True,
    language: Optional[str] = None
) -> AudioUnderstandingService:
    """创建音频理解服务"""
    return AudioUnderstandingService(
        whisper_model=whisper_model,
        use_cuda=use_cuda,
        language=language
    )


def create_audio_chunker(
    chunk_duration: float = 30.0,
    overlap: float = 2.0,
    min_duration: float = 5.0
) -> AudioChunker:
    """创建音频分块器"""
    return AudioChunker(
        chunk_duration=chunk_duration,
        overlap=overlap,
        min_duration=min_duration
    )


# ========== 示例使用 ==========

if __name__ == "__main__":
    async def main():
        service = create_audio_understanding_service()
        audio_path = "example.wav"

        if os.path.exists(audio_path):
            # 处理音频
            index = await service.process_audio(audio_path)
            print(f"Processed {index.duration:.1f}s audio, {len(index.segments)} segments")

            # 检索
            import numpy as np
            results = await service.retrieve_audio(
                "人工智能",
                np.random.randn(512),
                index,
                top_k=5
            )

            for r in results:
                print(f"[{r.timestamp}] {r.snippet[:100]}... (score: {r.similarity:.3f})")

    asyncio.run(main())
