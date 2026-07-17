"""
将所有的dimension划分成subspace, 然后每个subspace独立去做vector quantization
1:给定一个包含N个总向量的数据集(设定为 d dimension),我们首先将每个向量划分为M个子向量(也称为子空间),每个子空间的dimension为d/M。
这些子向量的长度不必完全相同,但实际情况下,推荐使用相同的长度(方便代码实现)。
2:对数据集中所有子向量使用k-均值(或其他聚类算法)。这会为每个子空间给出一组 K个质心, 每个质心都将被分配其唯一的ID。
3:在计算出所有质心后,我们将用其最近质心的ID替换原始数据集中的所有子向量。
4:给定一个新的 d dimension的向量,分成M个子向量,分别对应每个子向量找到对应的质心,然后用质心ID组合起来。
"""


import numpy as np
from sklearn.cluster import KMeans


class ProductQuantization:
    def __init__(self, M=16, K=256):
        self._dataset = None
        self._centroids = None
        self.M = M
        self.K = K

    def create(self, data_set):
        """PQ based on input dataset"""
        sub_len =  data_set.shape[1] // self.M

        # Used to store quantized central points
        self._centroids = np.empty((self.M, self.K, sub_len), dtype=np.float64)
        self._dataset = np.empty((data_set.shape[0], self.M), dtype=np.uint8)

        for m in range(self.M):
            # Train K center points for each M subspace
            sub_space = data_set[:, m * sub_len : (m + 1) * sub_len]
            kmeans = KMeans(n_clusters=self.K, init="k-means++", max_iter=32)
            kmeans.fit(sub_space)

            # Gets the centers all subspaces of each vector
            self._centroids[m, :, :] = kmeans.cluster_centers_
            self._dataset[:, m] = np.uint8(kmeans.labels_)

    def quantize(self, vectors):
        """Quantizes the input vector based on PQ parameters"""
        quantize = np.empty((vectors.shape[0], self.M), dtype=np.uint8)
        for i in range(vectors.shape[0]):
            for m in range(self.M):
                sub_len = vectors.shape[1] // self.M
                # Gets all centers of the current subspace
                centroids = self._centroids[m, :, :]
                sub_vector = vectors[i, m * sub_len: (m + 1) * sub_len]
                distances = np.linalg.norm(sub_vector - centroids, axis=1)
                quantize[i, m] = np.argmin(distances)

        return quantize

    def restore(self, quantized_vectors):
        """Restores the original vector using PQ parameters."""
        restored_vectors = []
        for i in range(quantized_vectors.shape[0]):
            restored_vector = np.hstack([self._centroids[m, quantized_vectors[i, m], :] for m in range(self.M)])
            restored_vectors.append(restored_vector)
        return np.array(restored_vectors)

    def dataset(self):
        try:
            if self._dataset:
                return self._dataset
        except:
            raise ValueError("Call ProductQuantization.create() first")


if __name__ == "__main__":
    dataset = np.random.normal(size=(1000, 128))
    pq = ProductQuantization()
    pq.create(dataset)

    # quantize a new vector
    v1 = np.random.normal(size=(2, 128))
    print(v1)
    quantized = pq.quantize(v1)
    print(quantized)

    # restore to original dimension vector (using centroids)
    restored_vectors = pq.restore(quantized)
    print(restored_vectors)
