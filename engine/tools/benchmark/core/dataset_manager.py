#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
数据集管理器 - 下载、缓存、加载 ANN Benchmarks 数据集
"""

import hashlib
import os
import sys
from pathlib import Path
from dataclasses import dataclass
from typing import Optional, Dict, Any, List

import numpy as np  # 必需依赖

try:
    import h5py
    HAS_H5PY = True
except ImportError:
    HAS_H5PY = False

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False


@dataclass
class BenchmarkDataset:
    """基准测试数据集"""
    name: str
    dimension: int
    distance: str
    train: np.ndarray
    test: np.ndarray
    neighbors: np.ndarray
    train_size: int
    test_size: int


class DatasetManager:
    """数据集管理器"""

    def __init__(self, data_dir: Optional[str] = None):
        if data_dir is None:
            # 使用相对于当前文件的路径
            self.data_dir = Path(__file__).parent.parent / "data"
        else:
            self.data_dir = Path(data_dir)
        self.data_dir.mkdir(parents=True, exist_ok=True)

        # 加载数据集配置
        self.config = self._load_config()

    def _load_config(self) -> Dict[str, Any]:
        """加载数据集配置"""
        config_path = Path(__file__).parent.parent / "config" / "datasets.yaml"
        if HAS_YAML and config_path.exists():
            try:
                with open(config_path, 'r', encoding='utf-8') as f:
                    return yaml.safe_load(f)
            except Exception as e:
                print(f"[Warning] Failed to load config: {e}", file=sys.stderr)

        # 默认配置
        return {
            "datasets": {
                "fashion-mnist-784-euclidean": {
                    "dimension": 784,
                    "distance": "euclidean",
                    "size": 60000,
                    "test_size": 10000,
                    "url": "http://ann-benchmarks.com/fashion-mnist-784-euclidean.hdf5"
                },
                "glove-100-angular": {
                    "dimension": 100,
                    "distance": "angular",
                    "size": 1183514,
                    "test_size": 10000,
                    "url": "http://ann-benchmarks.com/glove-100-angular.hdf5"
                }
            }
        }

    def _get_cache_path(self, name: str) -> Path:
        """获取缓存路径"""
        return self.data_dir / f"{name}.npz"

    def _download_dataset(self, name: str, url: str) -> str:
        """下载数据集到临时文件"""
        import urllib.request
        import tempfile

        print(f"[DatasetManager] Downloading {name} from {url}...")

        # 创建临时文件
        fd, temp_path = tempfile.mkstemp(suffix=".hdf5")
        os.close(fd)

        try:
            urllib.request.urlretrieve(url, temp_path)
            print(f"[DatasetManager] Downloaded to: {temp_path}")
            return temp_path
        except Exception as e:
            if os.path.exists(temp_path):
                os.remove(temp_path)
            raise RuntimeError(f"Failed to download {name}: {e}")

    def _parse_hdf5(self, hdf5_path: str) -> Dict[str, np.ndarray]:
        """解析 HDF5 文件"""
        if not HAS_H5PY:
            raise RuntimeError(
                "h5py is required to parse HDF5 files. "
                "Install with: pip install h5py"
            )

        data = {}
        with h5py.File(hdf5_path, 'r') as f:
            for key in ['train', 'test', 'neighbors']:
                if key in f:
                    data[key] = f[key][:]
                    print(f"[DatasetManager] Loaded {key}: {data[key].shape}")

        return data

    def load(self, name: str, force_download: bool = False) -> BenchmarkDataset:
        """
        加载数据集

        Args:
            name: 数据集名称
            force_download: 是否强制重新下载

        Returns:
            BenchmarkDataset 对象
        """
        if name not in self.config.get("datasets", {}):
            available = list(self.config.get("datasets", {}).keys())
            raise ValueError(
                f"Unknown dataset: {name}. Available: {available}"
            )

        config = self.config["datasets"][name]
        cache_path = self._get_cache_path(name)

        # 尝试从缓存加载
        if not force_download and cache_path.exists():
            print(f"[DatasetManager] Loading {name} from cache: {cache_path}")
            try:
                data = np.load(cache_path)
                return BenchmarkDataset(
                    name=name,
                    dimension=config["dimension"],
                    distance=config["distance"],
                    train=data["train"],
                    test=data["test"],
                    neighbors=data["neighbors"],
                    train_size=len(data["train"]),
                    test_size=len(data["test"])
                )
            except Exception as e:
                print(f"[Warning] Failed to load from cache: {e}")
                print(f"[DatasetManager] Re-downloading...")

        # 下载并解析
        hdf5_path = self._download_dataset(name, config["url"])
        try:
            raw_data = self._parse_hdf5(hdf5_path)

            # 保存到缓存
            print(f"[DatasetManager] Saving {name} to cache: {cache_path}")
            np.savez_compressed(
                cache_path,
                train=raw_data["train"],
                test=raw_data["test"],
                neighbors=raw_data["neighbors"]
            )

            return BenchmarkDataset(
                name=name,
                dimension=config["dimension"],
                distance=config["distance"],
                train=raw_data["train"],
                test=raw_data["test"],
                neighbors=raw_data["neighbors"],
                train_size=len(raw_data["train"]),
                test_size=len(raw_data["test"])
            )
        finally:
            # 清理临时文件
            if os.path.exists(hdf5_path):
                os.remove(hdf5_path)

    def list_datasets(self) -> List[str]:
        """列出所有可用的数据集"""
        return list(self.config.get("datasets", {}).keys())

    def download_all(self) -> None:
        """下载所有数据集"""
        for name in self.config.get("datasets", {}):
            try:
                self.load(name)
                print(f"[DatasetManager] ✓ {name}")
            except Exception as e:
                print(f"[DatasetManager] ✗ {name}: {e}")
