#!/usr/bin/env python3
"""
适配器模式 (Adapter Pattern) 示例

目标 (Target): 客户端期望的接口
被适配者 (Adaptee): 需要适配的现有类
适配器 (Adapter): 将 Adaptee 的接口转换为 Target 接口

本示例模拟一个媒体播放器系统：
- Target: MediaPlayer 接口 (play_audio)
- Adaptee: AdvancedMediaPlayer (play_mp4, play_vlc)
- Adapter: MediaAdapter 将 AdvancedMediaPlayer 适配为 MediaPlayer
"""

from abc import ABC, abstractmethod
from typing import List, Optional


# ==================== 目标接口 (Target) ====================

class MediaPlayer(ABC):
    """客户端期望的通用媒体播放器接口"""

    @abstractmethod
    def play_audio(self, filename: str) -> str:
        """播放音频文件"""
        pass

    @abstractmethod
    def get_supported_formats(self) -> List[str]:
        """获取支持的格式列表"""
        pass


# ==================== 被适配者 (Adaptee) ====================

class AdvancedMediaPlayer:
    """高级媒体播放器，拥有不兼容的接口"""

    def play_mp4(self, filename: str) -> str:
        """播放 MP4 文件（只有视频播放能力）"""
        return f"正在播放 MP4 视频: {filename} (音视频同步)"

    def play_vlc(self, filename: str) -> str:
        """播放 VLC 文件"""
        return f"正在播放 VLC 媒体: {filename} (支持字幕)"

    def play_avi(self, filename: str) -> str:
        """播放 AVI 文件"""
        return f"正在播放 AVI 文件: {filename} (旧格式兼容)"

    def get_format_list(self) -> List[str]:
        """返回支持的格式（字符串列表）"""
        return ["mp4", "vlc", "avi"]


class LegacyAudioPlayer:
    """旧的音频播放器，只能播放 mp3"""

    def play_mp3(self, filename: str) -> str:
        return f"正在播放 MP3 音频: {filename}"

    def get_extension(self) -> str:
        return "mp3"


# ==================== 对象适配器 (Object Adapter) ====================

class MediaAdapter(MediaPlayer):
    """
    对象适配器：通过组合 (composition) 实现适配

    持有 AdvancedMediaPlayer 的引用，将其接口适配为 MediaPlayer
    """

    # 格式到方法的映射
    _PLAY_METHODS = {
        "mp4": "play_mp4",
        "vlc": "play_vlc",
        "avi": "play_avi",
    }

    def __init__(self) -> None:
        self._advanced_player = AdvancedMediaPlayer()

    def play_audio(self, filename: str) -> str:
        """将 play_audio 请求转发给 AdvancedMediaPlayer 的适当方法"""
        ext = self._get_extension(filename)
        method_name = self._PLAY_METHODS.get(ext)

        if method_name is None:
            raise ValueError(f"不支持的格式: {ext}")

        method = getattr(self._advanced_player, method_name)
        return method(filename)

    def get_supported_formats(self) -> List[str]:
        """委托给被适配者的对应方法"""
        return self._advanced_player.get_format_list()

    @staticmethod
    def _get_extension(filename: str) -> str:
        """提取文件扩展名"""
        parts = filename.rsplit(".", 1)
        if len(parts) < 2:
            raise ValueError(f"文件名缺少扩展名: {filename}")
        return parts[1].lower()


# ==================== 类适配器 (Class Adapter) ====================

class LegacyToMediaAdapter(MediaPlayer, LegacyAudioPlayer):
    """
    类适配器：通过多继承实现适配

    注意：Python 支持多继承，因此可以实现类适配器
    但这种方式不如对象适配器灵活
    """

    def play_audio(self, filename: str) -> str:
        ext = self._get_extension(filename)
        if ext == "mp3":
            return self.play_mp3(filename)
        else:
            raise ValueError(f"LegacyToMediaAdapter 只支持 mp3，收到: {ext}")

    def get_supported_formats(self) -> List[str]:
        return [self.get_extension()]

    @staticmethod
    def _get_extension(filename: str) -> str:
        parts = filename.rsplit(".", 1)
        if len(parts) < 2:
            raise ValueError(f"文件名缺少扩展名: {filename}")
        return parts[1].lower()


# ==================== 客户端代码 (Client) ====================

class AudioClient:
    """
    客户端：只依赖 MediaPlayer 接口
    不需要知道底层是原生实现还是适配器
    """

    def __init__(self, player: MediaPlayer) -> None:
        self._player = player

    def play_song(self, filename: str) -> str:
        """播放歌曲，客户端只关心 MediaPlayer 接口"""
        return self._player.play_audio(filename)

    def show_supported(self) -> str:
        """显示支持的格式"""
        formats = self._player.get_supported_formats()
        return f"支持的格式: {', '.join(formats)}"


# ==================== 原生实现 ====================

class NativeMp3Player(MediaPlayer):
    """原生实现了 MediaPlayer 接口的播放器（无需适配）"""

    def play_audio(self, filename: str) -> str:
        return f"原生播放 MP3: {filename}"

    def get_supported_formats(self) -> List[str]:
        return ["mp3"]


# ==================== 主函数：演示适配器模式 ====================

def main() -> None:
    print("=" * 60)
    print("适配器模式 (Adapter Pattern) 演示")
    print("=" * 60)

    # 1. 原生实现：直接可用
    print("\n[场景 1] 原生 MediaPlayer 实现")
    native = NativeMp3Player()
    client = AudioClient(native)
    print(f"  {client.show_supported()}")
    print(f"  {client.play_song('song.mp3')}")
    print()

    # 2. 对象适配器：将 AdvancedMediaPlayer 适配为 MediaPlayer
    print("[场景 2] 对象适配器 (Object Adapter)")
    adapter = MediaAdapter()
    client2 = AudioClient(adapter)
    print(f"  {client2.show_supported()}")

    for fname in ["movie.mp4", "video.vlc", "archive.avi"]:
        try:
            print(f"  {client2.play_song(fname)}")
        except ValueError as e:
            print(f"  错误: {e}")
    print()

    # 3. 类适配器：通过多继承适配旧播放器
    print("[场景 3] 类适配器 (Class Adapter)")
    class_adapter = LegacyToMediaAdapter()
    client3 = AudioClient(class_adapter)
    print(f"  {client3.show_supported()}")
    print(f"  {client3.play_song('old_song.mp3')}")
    print()

    # 4. 适配器还可以包装第三方库
    print("[场景 4] 适配器隔离第三方依赖")
    print("  MediaAdapter 封装了 AdvancedMediaPlayer 的细节")
    print("  更换底层库时只需修改适配器，客户端代码不变")
    print()

    print("=" * 60)
    print("总结")
    print("  对象适配器 (MediaAdapter): 通过组合，灵活适配多种格式")
    print("  类适配器 (LegacyToMediaAdapter): 通过继承，只能适配单一来源")
    print("  推荐优先使用对象适配器 (组合优于继承)")
    print("=" * 60)


if __name__ == "__main__":
    main()
