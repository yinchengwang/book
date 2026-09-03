"""
@file video_understanding.py
@brief 视频理解与检索服务

功能:
1. 视频 → 关键帧采样 + 音频转录
2. 关键帧 → CLIP/SigLIP embedding
3. 转录 → 文本 chunk
4. 建立时间索引
"""

import asyncio
import os
import json
from dataclasses import dataclass, field
from typing import List, Dict, Any, Optional, Tuple, Union
from enum import Enum
import numpy as np

try:
    import cv2
    CV2_AVAILABLE = True
except ImportError:
    CV2_AVAILABLE = False


class VideoSegmentType(Enum):
    """视频片段类型"""
    SCENE = "scene"
    SHOT = "shot"
    SLIDE = "slide"
    DEMO = "demo"
    TALKING = "talking"


@dataclass
class VideoSegment:
    """视频片段"""
    segment_id: str
    start_time: float  # 秒
    end_time: float    # 秒
    duration: float = 0.0

    # 内容
    frame_embedding: Optional[np.ndarray] = None
    transcript_chunk: str = ""
    summary: str = ""

    # 元数据
    segment_type: VideoSegmentType = VideoSegmentType.SCENE
    scene_id: Optional[int] = None
    key_frame_indices: List[int] = field(default_factory=list)

    # 质量
    motion_score: float = 0.0
    audio_quality: float = 1.0

    def to_dict(self) -> Dict[str, Any]:
        return {
            "segment_id": self.segment_id,
            "start_time": self.start_time,
            "end_time": self.end_time,
            "duration": self.duration,
            "transcript_chunk": self.transcript_chunk,
            "summary": self.summary,
            "segment_type": self.segment_type.value,
            "motion_score": self.motion_score,
        }


@dataclass
class VideoIndex:
    """视频索引"""
    video_path: str
    duration: float = 0.0
    resolution: Tuple[int, int] = (0, 0)
    fps: float = 0.0

    segments: List[VideoSegment] = field(default_factory=list)
    frame_embeddings: Optional[np.ndarray] = None  # [N, D]

    # 全文检索索引
    text_chunks: List[str] = field(default_factory=list)
    text_embeddings: Optional[np.ndarray] = None  # [M, D]

    metadata: Dict[str, Any] = field(default_factory=dict)

    def get_segment_by_time(self, timestamp: float) -> Optional[VideoSegment]:
        """根据时间戳获取片段"""
        for seg in self.segments:
            if seg.start_time <= timestamp <= seg.end_time:
                return seg
        return None

    def search_segments(
        self,
        query_embedding: np.ndarray,
        top_k: int = 5
    ) -> List[Tuple[VideoSegment, float]]:
        """根据 embedding 搜索片段"""
        if self.frame_embeddings is None:
            return []

        # 计算相似度
        similarities = np.dot(self.frame_embeddings, query_embedding)
        top_indices = np.argsort(similarities)[-top_k:][::-1]

        return [
            (self.segments[i], float(similarities[i]))
            for i in top_indices if i < len(self.segments)
        ]

    def to_dict(self) -> Dict[str, Any]:
        return {
            "video_path": self.video_path,
            "duration": self.duration,
            "resolution": self.resolution,
            "segment_count": len(self.segments),
            "segments": [s.to_dict() for s in self.segments],
        }


@dataclass
class VideoRetrievalResult:
    """视频检索结果"""
    segment: VideoSegment
    similarity: float
    timestamp: str
    snippet: str
    thumbnail_path: Optional[str] = None


class SceneDetector:
    """场景检测器"""

    def __init__(self, threshold: float = 30.0):
        """
        @param threshold: 帧差异阈值
        """
        self.threshold = threshold

    def detect(self, video_path: str) -> List[Dict[str, Any]]:
        """
        检测场景变化

        @param video_path: 视频路径
        @return 场景列表 [{"start": t1, "end": t2, "timestamp": t}]
        """
        if not CV2_AVAILABLE:
            return self._fallback_scenes(video_path)

        scenes = []
        cap = cv2.VideoCapture(video_path)

        fps = cap.get(cv2.CAP_PROP_FPS)
        total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))

        prev_frame = None
        scene_start = 0

        frame_idx = 0
        while cap.isOpened():
            ret, frame = cap.read()
            if not ret:
                break

            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

            if prev_frame is not None:
                # 计算帧差异
                diff = cv2.norm(gray, prev_frame, cv2.NORM_L2)

                if diff > self.threshold:
                    # 场景切换
                    timestamp = frame_idx / fps
                    scenes.append({
                        "start": scene_start / fps,
                        "end": timestamp,
                        "timestamp": timestamp,
                        "frame_idx": frame_idx,
                    })
                    scene_start = frame_idx

            prev_frame = gray
            frame_idx += 1

        cap.release()

        # 添加最后一个场景
        if scene_start < total_frames:
            scenes.append({
                "start": scene_start / fps,
                "end": total_frames / fps,
                "timestamp": total_frames / fps,
                "frame_idx": total_frames,
            })

        return scenes

    def _fallback_scenes(self, video_path: str) -> List[Dict[str, Any]]:
        """无 OpenCV 时的降级方案"""
        # 每 30 秒一个场景
        return [{"start": 0, "end": 30, "timestamp": 0}]


class VideoUnderstandingService:
    """
    视频理解与检索服务

    处理流程:
    1. 场景检测
    2. 关键帧采样
    3. 音频转录 (Whisper)
    4. 批量 embedding
    5. 构建索引
    """

    def __init__(
        self,
        frame_model: str = "laion/CLIP-ViT-H-14-laion2B-s32B-b79K",
        whisper_model: str = "large-v3",
        use_cuda: bool = True
    ):
        """
        @param frame_model: 帧 embedding 模型
        @param whisper_model: Whisper 模型大小
        @param use_cuda: 是否使用 GPU
        """
        self.frame_model = frame_model
        self.whisper_model = whisper_model
        self.use_cuda = use_cuda
        self.scene_detector = SceneDetector()

        # 模型实例 (延迟加载)
        self._clip_model = None
        self._whisper_model = None
        self._whisper_processor = None

    @property
    def clip_model(self):
        """延迟加载 CLIP 模型"""
        if self._clip_model is None:
            # TODO: 加载 CLIP
            pass
        return self._clip_model

    @property
    def whisper(self):
        """延迟加载 Whisper 模型"""
        if self._whisper_model is None:
            # TODO: 加载 Whisper
            # from faster_whisper import WhisperModel
            # self._whisper_model = WhisperModel(self.whisper_model, use_cuda=self.use_cuda)
            pass
        return self._whisper_model

    async def process_video(
        self,
        video_path: str,
        sample_fps: float = 1.0,
        max_segments: int = 100
    ) -> VideoIndex:
        """
        处理视频

        @param video_path: 视频路径
        @param sample_fps: 采样帧率
        @param max_segments: 最大片段数
        @return 视频索引
        """
        # 1. 获取视频元数据
        metadata = self._get_video_metadata(video_path)

        # 2. 场景检测
        scenes = self.scene_detector.detect(video_path)

        # 3. 每场景采样关键帧
        frames = await self._extract_key_frames(video_path, scenes, sample_fps)

        # 4. 音频转录
        transcript = await self._transcribe_audio(video_path)

        # 5. 对齐转录和帧
        segments = self._align_transcript_frames(
            transcript, frames, scenes, metadata["duration"]
        )

        # 6. 限制片段数
        if len(segments) > max_segments:
            segments = self._merge_segments(segments, max_segments)

        # 7. 批量 embedding
        frame_embeddings = await self._encode_frames(frames)

        # 8. 文本 embedding
        text_chunks = [seg.transcript_chunk for seg in segments]
        text_embeddings = await self._encode_text(text_chunks)

        # 9. 构建索引
        return VideoIndex(
            video_path=video_path,
            duration=metadata["duration"],
            resolution=metadata["resolution"],
            fps=metadata["fps"],
            segments=segments,
            frame_embeddings=frame_embeddings,
            text_chunks=text_chunks,
            text_embeddings=text_embeddings,
            metadata=metadata,
        )

    def _get_video_metadata(self, video_path: str) -> Dict[str, Any]:
        """获取视频元数据"""
        metadata = {
            "duration": 0.0,
            "resolution": (1920, 1080),
            "fps": 30.0,
            "codec": "unknown",
        }

        if not os.path.exists(video_path):
            return metadata

        if CV2_AVAILABLE:
            cap = cv2.VideoCapture(video_path)
            metadata["fps"] = cap.get(cv2.CAP_PROP_FPS)
            metadata["frame_count"] = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
            metadata["duration"] = metadata["frame_count"] / metadata["fps"]
            metadata["resolution"] = (
                int(cap.get(cv2.CAP_PROP_FRAME_WIDTH)),
                int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT)),
            )
            cap.release()

        return metadata

    async def _extract_key_frames(
        self,
        video_path: str,
        scenes: List[Dict[str, Any]],
        sample_fps: float
    ) -> List[np.ndarray]:
        """提取关键帧"""
        frames = []

        if not CV2_AVAILABLE:
            return frames

        cap = cv2.VideoCapture(video_path)
        fps = cap.get(cv2.CAP_PROP_FPS)

        for scene in scenes:
            # 在场景中间采样一帧
            mid_time = (scene["start"] + scene["end"]) / 2
            frame_idx = int(mid_time * fps)

            cap.set(cv2.CAP_PROP_POS_FRAMES, frame_idx)
            ret, frame = cap.read()

            if ret:
                # BGR -> RGB
                frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                frames.append(frame_rgb)

        cap.release()
        return frames

    async def _transcribe_audio(self, video_path: str) -> Dict[str, Any]:
        """音频转录"""
        # TODO: 使用 Whisper 转录
        # transcript = self.whisper.transcribe(video_path, word_timestamps=True)
        return {
            "text": "",
            "segments": [],
            "language": "en",
        }

    def _align_transcript_frames(
        self,
        transcript: Dict[str, Any],
        frames: List[np.ndarray],
        scenes: List[Dict[str, Any]],
        duration: float
    ) -> List[VideoSegment]:
        """对齐转录和帧"""
        segments = []

        if not frames:
            return segments

        # 创建片段
        frame_idx = 0
        for i, scene in enumerate(scenes):
            if frame_idx >= len(frames):
                break

            start_time = scene["start"]
            end_time = scene["end"]

            # 找到对应的转录片段
            transcript_text = ""
            for seg in transcript.get("segments", []):
                if seg["start"] >= start_time and seg["end"] <= end_time:
                    transcript_text += seg["text"] + " "

            segment = VideoSegment(
                segment_id=f"seg_{i:04d}",
                start_time=start_time,
                end_time=end_time,
                duration=end_time - start_time,
                frame_embedding=None,  # 稍后填充
                transcript_chunk=transcript_text.strip(),
                scene_id=i,
                segment_type=VideoSegmentType.SCENE,
            )
            segments.append(segment)
            frame_idx += 1

        return segments

    def _merge_segments(
        self,
        segments: List[VideoSegment],
        target_count: int
    ) -> List[VideoSegment]:
        """合并片段以达到目标数量"""
        if len(segments) <= target_count:
            return segments

        # 简单策略: 合并相邻片段
        merge_ratio = len(segments) / target_count
        merged = []

        i = 0
        while i < len(segments):
            # 计算要合并多少个
            count = max(1, int(merge_ratio))
            count = min(count, len(segments) - i)

            if count == 1:
                merged.append(segments[i])
                i += 1
            else:
                # 合并多个片段
                start_time = segments[i].start_time
                end_time = segments[i + count - 1].end_time
                transcript = " ".join(s.transcript_chunk for s in segments[i:i+count])

                merged_seg = VideoSegment(
                    segment_id=f"merged_{len(merged):04d}",
                    start_time=start_time,
                    end_time=end_time,
                    duration=end_time - start_time,
                    transcript_chunk=transcript,
                    segment_type=segments[i].segment_type,
                )
                merged.append(merged_seg)
                i += count

        return merged

    async def _encode_frames(self, frames: List[np.ndarray]) -> Optional[np.ndarray]:
        """编码帧"""
        if not frames:
            return None

        # TODO: 使用 CLIP 模型编码
        # embeddings = self.clip_model.encode_images(frames)
        return None

    async def _encode_text(self, texts: List[str]) -> Optional[np.ndarray]:
        """编码文本"""
        if not texts:
            return None

        # TODO: 使用文本编码器
        return None

    async def retrieve_video(
        self,
        query: str,
        query_embedding: np.ndarray,
        video_index: VideoIndex,
        top_k: int = 5
    ) -> List[VideoRetrievalResult]:
        """
        检索视频片段

        @param query: 查询文本
        @param query_embedding: 查询 embedding
        @param video_index: 视频索引
        @param top_k: 返回结果数
        @return 检索结果
        """
        results = []

        # 1. 基于帧 embedding 检索
        if video_index.frame_embeddings is not None:
            frame_results = video_index.search_segments(query_embedding, top_k)
            for seg, sim in frame_results:
                results.append(VideoRetrievalResult(
                    segment=seg,
                    similarity=sim,
                    timestamp=f"{seg.start_time:.1f}s - {seg.end_time:.1f}s",
                    snippet=seg.transcript_chunk[:200] + "..." if len(seg.transcript_chunk) > 200 else seg.transcript_chunk,
                ))

        # 2. 基于文本检索 (如果结果不够)
        if len(results) < top_k and video_index.text_embeddings is not None:
            text_similarities = np.dot(video_index.text_embeddings, query_embedding)
            top_text_indices = np.argsort(text_similarities)[-top_k:][::-1]

            for idx in top_text_indices:
                if idx < len(video_index.segments):
                    seg = video_index.segments[idx]
                    # 检查是否已存在
                    if not any(r.segment.segment_id == seg.segment_id for r in results):
                        results.append(VideoRetrievalResult(
                            segment=seg,
                            similarity=float(text_similarities[idx]),
                            timestamp=f"{seg.start_time:.1f}s - {seg.end_time:.1f}s",
                            snippet=seg.transcript_chunk[:200],
                        ))

        # 按相似度排序
        results.sort(key=lambda x: x.similarity, reverse=True)

        return results[:top_k]

    async def generate_video_summary(
        self,
        video_index: VideoIndex,
        max_length: int = 500
    ) -> str:
        """生成视频摘要"""
        # 收集所有转录
        all_text = []
        for seg in video_index.segments:
            if seg.transcript_chunk:
                all_text.append(seg.transcript_chunk)

        if not all_text:
            return "视频内容无法转录"

        # TODO: 使用 LLM 生成摘要
        full_text = " ".join(all_text)
        # summary = llm.summarize(full_text, max_length)
        return full_text[:max_length] + "..." if len(full_text) > max_length else full_text


# ========== Factory ==========

def create_video_understanding_service(
    frame_model: str = "laion/CLIP-ViT-H-14-laion2B-s32B-b79K",
    whisper_model: str = "large-v3",
    use_cuda: bool = True
) -> VideoUnderstandingService:
    """创建视频理解服务"""
    return VideoUnderstandingService(
        frame_model=frame_model,
        whisper_model=whisper_model,
        use_cuda=use_cuda
    )


# ========== 示例使用 ==========

if __name__ == "__main__":
    async def main():
        service = create_video_understanding_service()

        # 处理视频
        video_path = "example.mp4"

        if os.path.exists(video_path):
            # 处理
            index = await service.process_video(video_path)
            print(json.dumps(index.to_dict(), indent=2, ensure_ascii=False))

            # 检索
            results = await service.retrieve_video(
                "人工智能的应用",
                np.random.randn(512),
                index,
                top_k=5
            )

            for r in results:
                print(f"[{r.timestamp}] {r.snippet[:100]}... (score: {r.similarity:.3f})")

    asyncio.run(main())
