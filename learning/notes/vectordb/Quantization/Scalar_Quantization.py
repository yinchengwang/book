"""
Scalar Quantization是最简单易懂的quantization技术,它通过将大空间的向量转换成相对小空间的向量来减少整体的存储空间。
通常是将浮点数(比如 f32)向量量化成整数(比如 int8)向量(存储的优化比是 4X)。
因此,scalar quantization是通过牺牲了计算精度来提升存储和查询效率。

针对每一个dimension, 遍历所有的数字得到min和max值, 然后将min值映射到0(int8的min值), max值映射到255(int8的max值)。
然后, 通过(max - min)/255计算出step值, 相当于把min到max划分成了255个桶, 然后对原先的数字进行桶排序, 将原先向量中的浮点数替换成桶的ID数, 就做到了f32到int8的映射。
"""
import numpy as np


class ScalarQuantizer:
    def __init__(self):
        self._steps = None
        self._starts = None
        self._dataset = None

    def create(self, data_set):
        """Calculates and stores SQ parameters based on the input dataset."""
        self._starts = np.min(data_set, axis=0)
        self._steps = (np.max(data_set, axis=0) - self._starts) / 255
        self._dataset = np.uint8((data_set - self._starts) / self._steps)

    def quantize(self, vector):
        """Quantizes the input vector based on SQ parameters"""
        return np.uint8((vector - self._starts) / self._steps)

    def restore(self, vector):
        """Restores the original vector using SQ parameters."""
        return (vector * self._steps) + self._starts

    @property
    def dataset(self):
        if self._dataset:
            return self._dataset
        raise ValueError("Call ScalarQuantizer.create() first")


if __name__ == "__main__":
    dataset = np.random.normal(size=(1000, 128))
    sq = ScalarQuantizer()
    sq.create(dataset)

    v1 = np.random.normal(size=(2, 128))
    print(v1)
    quantized = sq.quantize(v1)
    print(quantized)
