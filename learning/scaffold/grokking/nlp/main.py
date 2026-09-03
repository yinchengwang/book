"""
nlp — 自然语言处理

演示词嵌入（Word Embedding）、TextCNN 文本分类和情感分析。
纯 numpy 实现，无外部深度学习框架依赖。
"""

import numpy as np


# ════════════════════════════════════════════════════════════
# 1. 词嵌入（Word Embedding）
# ════════════════════════════════════════════════════════════
class WordEmbedding:
    """简单的词嵌入查找表"""
    def __init__(self, vocab_size, embedding_dim):
        self.vocab_size = vocab_size
        self.embedding_dim = embedding_dim
        # 随机初始化词向量
        self.embeddings = np.random.randn(vocab_size, embedding_dim) * 0.1
        self.word_to_idx = {}
        self.idx_to_word = {}

    def add_word(self, word, idx):
        self.word_to_idx[word] = idx
        self.idx_to_word[idx] = word

    def __getitem__(self, word):
        return self.embeddings[self.word_to_idx[word]]

    def cosine_similarity(self, word1, word2):
        """计算词向量的余弦相似度"""
        v1 = self[word1]
        v2 = self[word2]
        return np.dot(v1, v2) / (np.linalg.norm(v1) * np.linalg.norm(v2) + 1e-10)


def demo_word_embedding():
    """演示词嵌入的语义表示能力"""
    np.random.seed(42)
    words = ["king", "queen", "man", "woman", "apple", "orange", "car", "bus", "run", "walk"]
    embed = WordEmbedding(len(words), 8)
    for i, w in enumerate(words):
        embed.add_word(w, i)

    print("  词嵌入演示（8 维随机初始化）")
    print(f"  king 的词向量: {embed['king']}")
    print(f"  queen 的词向量: {embed['queen']}")
    print()

    # 相似度矩阵
    print("  词向量余弦相似度矩阵")
    print(f"  {'':>10}", end="")
    for w in words[:6]:
        print(f"  {w:>8}", end="")
    print()
    for w1 in words[:6]:
        print(f"  {w1:>10}", end="")
        for w2 in words[:6]:
            sim = embed.cosine_similarity(w1, w2)
            print(f"  {sim:>8.4f}", end="")
        print()
    print()

    # 类比推理（随机初始化下只是演示概念）
    print("  词向量类比推理（概念演示）")
    print("  king - man + woman ≈ queen  （语义偏移）")
    print("  向量运算:w(king) - w(man) + w(woman) = w(queen)")
    result_vec = embed["king"] - embed["man"] + embed["woman"]
    # 找最近的词
    sims = [np.dot(result_vec, embed.embeddings[i]) /
            (np.linalg.norm(result_vec) * np.linalg.norm(embed.embeddings[i]))
            for i in range(len(words))]
    nearest = np.argsort(sims)[::-1][:3]
    print(f"  最近邻词: {[embed.idx_to_word[i] for i in nearest]} (相似度={[f'{sims[i]:.3f}' for i in nearest]})")
    print()


# ════════════════════════════════════════════════════════════
# 2. TextCNN
# ════════════════════════════════════════════════════════════
class TextCNN:
    """简化版 TextCNN —— 1D 卷积 + 池化 + 全连接"""
    def __init__(self, vocab_size, embed_dim=8, num_filters=4, num_classes=2):
        self.embed_dim = embed_dim
        self.num_filters = num_filters
        self.num_classes = num_classes
        # 嵌入层
        self.embeddings = np.random.randn(vocab_size, embed_dim) * 0.1
        # 卷积核（Ngram 特征提取）
        self.kernels = {
            2: np.random.randn(num_filters, embed_dim) * 0.1,  # Bigram
            3: np.random.randn(num_filters, embed_dim) * 0.1,  # Trigram
        }
        # 全连接层
        n_kernels = len(self.kernels) * num_filters
        self.fc_w = np.random.randn(n_kernels, num_classes) * 0.1
        self.fc_b = np.zeros((1, num_classes))

    def _conv1d(self, emb_seq, kernel):
        """对嵌入序列执行 1D 卷积（简化——取最大值池化）"""
        seq_len = len(emb_seq)
        features = []
        for k_size, k_vec in kernel.items():
            k_features = []
            for f in range(self.num_filters):
                filter_out = []
                for pos in range(seq_len - k_size + 1):
                    # 滤波器和窗口的点积
                    window = emb_seq[pos:pos+k_size].reshape(-1)
                    filter_vec = k_vec[f]
                    # 扩展 filter 到 k_size * embed_dim
                    expanded = np.tile(filter_vec, k_size)
                    val = np.dot(window, expanded[:len(window)])
                    filter_out.append(val)
                # Max-over-time pooling
                k_features.append(max(filter_out) if filter_out else 0)
            features.extend(k_features)
        return np.array(features)

    def forward(self, tokens):
        """前向传播"""
        emb_seq = self.embeddings[tokens]
        features = self._conv1d(emb_seq, self.kernels)
        logits = features @ self.fc_w + self.fc_b
        # Softmax
        exp_logits = np.exp(logits - np.max(logits))
        probs = exp_logits / np.sum(exp_logits)
        return probs, features


def demo_textcnn():
    """TextCNN 文本分类演示"""
    np.random.seed(42)
    # 构建简单词汇表
    vocab = ["<pad>", "this", "movie", "is", "very", "good", "bad",
             "awesome", "terrible", "amazing", "boring", "great", "awful"]
    word_to_idx = {w: i for i, w in enumerate(vocab)}

    # 构建样例句子
    sentences = [
        [1, 2, 3, 5, 8],     # "this movie is good awesome" → 正面
        [1, 2, 3, 6, 9],     # "this movie is bad terrible" → 负面
        [1, 2, 3, 10, 11],   # "this movie is amazing great" → 正面
        [1, 2, 3, 12, 6],    # "this movie is awful bad" → 负面
    ]

    print("  TextCNN 文本分类演示")
    print(f"  词汇表大小: {len(vocab)}")
    print(f"  嵌入维度: 8")
    print(f"  卷积核: Bigram + Trigram, 各 {TextCNN.num_filters} 个")

    textcnn = TextCNN(vocab_size=len(vocab), embed_dim=8, num_filters=4)
    for i, sentence in enumerate(sentences):
        probs, features = textcnn.forward(np.array(sentence))
        sentiment = "正面" if np.argmax(probs) == 0 else "负面"
        words = [vocab[t] for t in sentence]
        print(f"  句子 {i+1}: {' '.join(words[1:])}")
        print(f"    特征向量前 4 维: {features[:4]}")
        print(f"    情感预测: {sentiment} (P(pos)={probs[0,0]:.3f}, P(neg)={probs[0,1]:.3f})")
    print()


# ════════════════════════════════════════════════════════════
# 3. 情感分析（简单字典方法）
# ════════════════════════════════════════════════════════════
def demo_sentiment_analysis():
    """演示基于情感词典的简单情感分析"""
    # 情感词典（简化）
    positive_words = {"good", "great", "awesome", "amazing", "excellent",
                      "wonderful", "fantastic", "love", "beautiful", "happy"}
    negative_words = {"bad", "terrible", "awful", "boring", "horrible",
                      "ugly", "hate", "sad", "angry", "disgusting"}
    intensifiers = {"very", "really", "extremely", "incredibly", "absolutely"}

    def analyze_sentiment(text):
        words = text.lower().split()
        score = 0
        pos_count, neg_count = 0, 0
        for i, w in enumerate(words):
            if w in intensifiers and i + 1 < len(words):
                continue
            if w in positive_words:
                # 检查是否有程度词
                if i > 0 and words[i-1] in intensifiers:
                    score += 2
                else:
                    score += 1
                pos_count += 1
            elif w in negative_words:
                if i > 0 and words[i-1] in intensifiers:
                    score -= 2
                else:
                    score -= 1
                neg_count += 1
        return score, pos_count, neg_count

    test_texts = [
        "I love this movie it is absolutely amazing",
        "This is a terrible boring movie really awful",
        "The weather is good today",
        "I hate this bad and disgusting product",
    ]

    print("  情感分析演示（基于情感词典）")
    for text in test_texts:
        score, pos, neg = analyze_sentiment(text)
        sentiment = "正面" if score > 0 else "负面" if score < 0 else "中性"
        print(f"  文本: \"{text}\"")
        print(f"    情感得分: {score:+d} (正面词:{pos}, 负面词:{neg}) → {sentiment}")
    print()


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  NLP — 词嵌入/TextCNN/情感分析          ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_word_embedding()
    demo_textcnn()
    demo_sentiment_analysis()

    print("[[OK]] NLP 演示完成")
